/* ====================================================================
 * Copyright (c) 1995-1998 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

#include "httpd.h"
#include "http_config.h"
#include "http_request.h"
#include "http_log.h"


typedef struct dir_cfg {
    int compress_content;
} dir_cfg;


module MODULE_VAR_EXPORT gzip_content_module;


/*
 * If a directory has a CompressContent command for it,
 * set the dir_cfg member appropriately so we know to try
 * and send compressed content from that directory.
 */
static const char *gzip_compress_content_cmd(cmd_parms *cmd, void *mconfig,
                                             char *arg)
{

    dir_cfg *cfg = (dir_cfg *) mconfig;

    if ((arg == NULL)||(*arg == '\0'))
        return NULL;

    if (!strcasecmp(arg, "Yes"))
    {
        cfg->compress_content = 1;
    }
    else if (!strcasecmp(arg, "No"))
    {
        cfg->compress_content = 0;
    }

    return NULL;
}


static const command_rec gzip_cmds[] =
{
    {
        "CompressContent",              /* directive name */
        gzip_compress_content_cmd,      /* config action routine */
        NULL,                           /* argument to include in call */
        OR_OPTIONS,                     /* where available */
        TAKE1,                          /* arguments */
        "CompressContent directive - one argument, YES or NO"
                                        /* directive description */
    },
    {NULL}
};


static void *create_gzip_dir_config(pool *p, char *dirspec)
{

    dir_cfg *cfg;

    /*
     * Allocate the space for our record from the pool supplied.
     */
    cfg = (dir_cfg *) ap_pcalloc(p, sizeof(dir_cfg));

    cfg->compress_content = 0;

    return (void *) cfg;
}


/*
 * Children always inherit from their parents unless
 * explicitly set by a CompressContent command.
 */
static void *merge_gzip_dir_config(pool *p, void *parent_conf,
                                   void *newloc_conf)
{

    dir_cfg *pconf = (dir_cfg *) parent_conf;
    dir_cfg *nconf = (dir_cfg *) newloc_conf;

    return (void *)newloc_conf;
}


/*
 * I would expect this utility function to already exist
 * somewhere in the ap_ functions, but I couldn't find it.
 * It searches the passed string (case insensitivly) for
 * the presence of the passed substring.  Returns a pointer
 * to the start of the substring if found, otherwise NULL.
 */
static char *find_sub_str(char *str, char *sub)
{
    int indx;
    char *ptr;

    if (!sub || !str)
        return NULL;

    if (*sub == '\0' || *str == '\0')
        return NULL;

    indx = ap_ind(str, *sub);
    while (indx >= 0)
    {
        ptr = &str[indx];
        if (strncasecmp(ptr, sub, strlen(sub)) == 0)
        {
            return ptr;
        }
        ptr++;
        indx = ap_ind(ptr, *sub);
    }
    return NULL;
}


/*
 * If se are supposed to try and send compressed content from
 * this directory, generate a subrequest to look for the compressed
 * content.  If found, slip it in in place of this request.
 */
static int set_gzip_content(request_rec *r)
{
    dir_cfg *cfg;
    char *accept_enc_hdr;
    struct stat finfo;
    int ret;
    int look_for_gzip = 0;

    /*
     * Get the configuration information for this directory
     * so we know whether to try and send compressed content from
     * here or not.
     */
    cfg = (dir_cfg *)ap_get_module_config(r->per_dir_config,
                                          &gzip_content_module);
    if ((cfg == NULL)||(cfg->compress_content == 0))
    {
        return DECLINED;
    }

    /*
     * If the original file was not found, there is no need
     * to look for a compressed version.
     */
    if (r->finfo.st_mode == 0)
    {
        return DECLINED;
    }

    /*
     * Pass up directory requests, they will later get redirected to
     * something like index.html, and we can handle them then.
     */
    if (S_ISDIR(r->finfo.st_mode))
    {
        return DECLINED;
    }

    /*
     * sub-requests are just made to check the existence of
     * a certain file and get its type, they never return
     * a data stream, so we don't want to request a compressed
     * version of them.
     */
    if (r->main != NULL)
    {
        return DECLINED;
    }

    /*
     * No point in redirecting to compressed content if you are
     * just requesting a header with no content.
     */
    if (r->header_only)
    {
        return DECLINED;
    }

    /*
     * Even if we are supposed to try and send compressed content
     * for this request from this directory, we can still
     * only do it is we were sent an Accept-encoding header for
     * gzip data.
     */
    accept_enc_hdr = ap_table_get(r->headers_in, "Accept-encoding");
    if (accept_enc_hdr)
    {
        /*
         * This substring search is rather broad, in that it
         * will send gzip data on any header with the gzip
         * substring.  This is to catch both gzip and x-gzip,
         * but it may in fact be the wrong thing to do, I'm
         * not sure yet.
         */
        if (find_sub_str(accept_enc_hdr, "gzip") != NULL)
        {
            look_for_gzip = 1;
        }
    }

    /*
     * Finally we know if we are looking for a gzipped version of this
     * file request.
     */
    if (look_for_gzip)
    {
        /*
         * The fastest thing to do here would be to just
         * slap a .gz on the end of the r->filename, stat it,
         * and if it exists go with it.  But this wouldn't cover
         * weird cases where the requested has permission to receive
         * foo.html, but not foo.html.gz.
         *
         * So, we generate a new sub-request to see if we have
         * permission to access the .gz file (if it even exists).
         */
        request_rec *sub_req;
        char *new_file;

        new_file = ap_pstrcat(r->pool, r->filename, ".gz", NULL);
        sub_req = ap_sub_req_lookup_file(new_file, r);

        /*
         * Either the file doesn't exist or permission was
         * refused.
         */
        if ((sub_req->status != HTTP_OK)||(sub_req->finfo.st_mode == 0))
        {
            ap_destroy_sub_req(sub_req);
            return DECLINED;
        }

        /* "fast redirect" code stolen from mod_negotiation.c */
        /* now do a "fast redirect" ... promote the sub_req into the main req */
        /* We need to tell POOL_DEBUG that we're guaranteeing that sub_req->pool
         * will exist as long as r->pool.  Otherwise we run into troubles
         * because some values in this request will be allocated in r->pool,
         * and others in sub_req->pool.
         */
        /*
         * As far as I can tell we have to do this because
         * ap_internal_redirect() cannot be called this early in a request.
         * sure would have been nice if there was a comment saying that
         * somewhere, and perhaps we need some other ap_redirect function
         * that can be called this early, so that if something is changed
         * in the request structure later, all this code doesn't break.
         */
        ap_pool_join(r->pool, sub_req->pool);
        r->filename = sub_req->filename;
        r->handler = sub_req->handler;
        r->content_type = sub_req->content_type;
        r->content_encoding = sub_req->content_encoding;
        r->content_languages = sub_req->content_languages;
        r->content_language = sub_req->content_language;
        r->finfo = sub_req->finfo;
        r->per_dir_config = sub_req->per_dir_config;
        /* copy output headers from subrequest, but leave negotiation headers */
        r->notes = ap_overlay_tables(r->pool, sub_req->notes, r->notes);
        r->headers_out = ap_overlay_tables(r->pool, sub_req->headers_out,
                                           r->headers_out);
        r->err_headers_out = ap_overlay_tables(r->pool,
                                               sub_req->err_headers_out,
                                               r->err_headers_out);
        r->subprocess_env = ap_overlay_tables(r->pool, sub_req->subprocess_env,
                                              r->subprocess_env);

        return OK;
    }

    return DECLINED;
}

module MODULE_VAR_EXPORT gzip_content_module =
{
    STANDARD_MODULE_STUFF,
    NULL,                       /* initializer */
    create_gzip_dir_config,     /* dir config creator */
    merge_gzip_dir_config,      /* dir config merger */
    NULL,                       /* server config */
    NULL,                       /* merge server config */
    gzip_cmds,                  /* command table */
    NULL,                       /* handlers */
    NULL,                       /* filename translation */
    NULL,                       /* check_user_id */
    NULL,                       /* check auth */
    NULL,                       /* check access */
    set_gzip_content,           /* type_checker */
    NULL,                       /* fixups */
    NULL,                       /* logger */
    NULL,                       /* header parser */
    NULL,                       /* child_init */
    NULL,                       /* child_exit */
    NULL                        /* post read-request */
};

