/* $Id: streaming.cpp,v 1.1 1998/09/25 18:01:41 ramiro%netscape.com Exp $
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include <client.h>
#include "streaming.h"
#include "QtContext.h"
#include <xp_str.h>
#include <xlate.h>
#include <xpgetstr.h>
#include "SaveAsDialog.h"
#include "DialogPool.h"
#include "QtBrowserContext.h"
 
#include <qwidget.h>


#if defined(_CC_MSVC_)
#define stat _stat
#endif

void fe_RaiseSynchronousURLDialog (MWContext *context, QWidget* parent,
				   const char *title);
void fe_LowerSynchronousURLDialog (MWContext *context);
void fe_StartProgressGraph ( MWContext* context );
void fe_url_exit (URL_Struct *url, int status, MWContext *context);
bool QTFE_StatReadWrite( const char *file_name, bool isFileP, bool existsP );
extern void fe_save_as_complete( PrintSetup* ps );
extern int fe_AwaitSynchronousURL( MWContext* context );

extern int QTFE_SAVE_AS_TYPE_ENCODING;
extern int QTFE_SAVE_AS_TYPE;
extern int QTFE_SAVE_AS_ENCODING;
extern int QTFE_SAVE_AS;
extern int QTFE_ERROR_READ_ONLY;
extern int QTFE_ERROR_OPENING_FILE;

/* Creates and returns a stream object which writes the data read to a
   file.  If the file has not been prompted for / opened, it prompts the
   user.
 */
NET_StreamClass *
fe_MakeSaveAsStream (int /*format_out*/, void * /*data_obj*/,
		     URL_Struct *url_struct, MWContext *context)
{
    struct save_as_data *sad = 0;
    NET_StreamClass* stream;
    
    if (url_struct->fe_data)
	{
	    sad = (struct save_as_data*)url_struct->fe_data;
	}
    else
	{
	    bool text_p = (url_struct->content_type &&
			      (!strcasecomp (url_struct->content_type, TEXT_HTML) ||
			       !strcasecomp (url_struct->content_type, TEXT_MDL) ||
			       !strcasecomp (url_struct->content_type, TEXT_PLAIN)));
	    sad = make_save_as_data (context, text_p, SaveAsDialog::Source, 
				     url_struct, 0 );
	    if (! sad) return 0;
	}
    
    url_struct->fe_data = 0;
    
    stream = (NET_StreamClass *) calloc (sizeof (NET_StreamClass), 1);
    if (!stream) return 0;
    
    stream->name           = "SaveAs";
    stream->complete       = fe_save_as_stream_complete_method;
    stream->abort          = fe_save_as_stream_abort_method;
    stream->put_block      = fe_save_as_stream_write_method;
    stream->is_write_ready = fe_save_as_stream_write_ready_method;
    stream->data_object    = sad;
    stream->window_id      = context;
    
    if (sad->insert_base_tag && XP_STRCMP(url_struct->content_type, TEXT_HTML)
	== 0)
	{
	    /*
	    ** This is here to that any relative URL's in the document
	    ** will continue to work even though we are moving the document
	    ** to another world.
	    */
	    fe_save_as_stream_write_method(stream, "<BASE HREF=", 11);
	    fe_save_as_stream_write_method(stream, url_struct->address,
					   XP_STRLEN(url_struct->address));
	    fe_save_as_stream_write_method(stream, ">\n", 2);
	}
    
    sad->content_length = url_struct->content_length;
    FE_SetProgressBarPercent (context, -1);
    if( url_struct->server_can_do_byteranges || url_struct->server_can_do_restart )
        url_struct->must_cache = TRUE;
#if 0
    /* Oops, this makes the size twice as large as it should be. */
    FE_GraphProgressInit (context, url_struct, sad->content_length);
#endif
    
    if (sad->use_dialog_p)
	{
	    /* make sure it is safe to open a new context */
	    if (NET_IsSafeForNewContext(url_struct))
		{
		    /* create a download frame here */
		    //		    XFE_Frame *parent;
		    //		    XFE_DownloadFrame *new_frame;
		    MWContext *new_context;
		    
		    //		    parent = ViewGlue_getFrame(XP_GetNonGridContext(context));
		    QtSaveToDiskContext* qc = new QtSaveToDiskContext( context );
// #warning Show download window here. Kalle.
		    //		    new_frame = new XFE_DownloadFrame(XtParent(parent->getBaseWidget()), parent);
		    new_context = qc->mwContext();
		    
		    /* Set the values for location and filename */
		    //		    if (url_struct->address)
		    //			new_frame->setAddress(url_struct->address);
		    //		    if (sad && sad->name)
		    //			new_frame->setDestination(sad->name);
		    
		    //		    new_frame->show();
		    
		    /* Set the url_count and enable all animation */
		    CONTEXT_DATA (new_context)->active_url_count = 1;
		    fe_StartProgressGraph (new_context);
		    
		    /* register this new context associated with this stream
		     * with the netlib
		     */
		    NET_SetNewContext(url_struct, new_context, fe_url_exit);
		    
		    /* Change the fe_data's context */
		    sad->context = new_context;
		}
	    else
		fe_RaiseSynchronousURLDialog (context, 
					      CONTEXT_DATA (context)->topLevelWidget(),
					      "saving");
		}
    
    return stream;
}

NET_StreamClass *
fe_MakeSaveAsStreamNoPrompt (int format_out, void *data_obj,
			     URL_Struct *url_struct, MWContext *context)
{
    struct save_as_data *sad;
    
    assert (context->prSetup);
    if (! context->prSetup)
	return 0;
    
    sad = (struct save_as_data *) malloc (sizeof (struct save_as_data));
    sad->context = context;
    sad->name = strdup (context->prSetup->filename);
    sad->file = context->prSetup->out;
    sad->type = SaveAsDialog::Source;
    sad->done = 0;
    sad->insert_base_tag = FALSE;
    sad->use_dialog_p = FALSE;
  
    url_struct->fe_data = sad;
    
    sad->content_length = url_struct->content_length;
    FE_GraphProgressInit (context, url_struct, sad->content_length);
    
    return fe_MakeSaveAsStream (format_out, data_obj, url_struct, context);
}

/* here is the stuff ripped out of commands.c, which is used to actually
   set up the stream.  It needs to be here instead of commands.c because
   we have do set values of the text fields, through member functions. */
#if defined(XP_UNIX)
extern char **fe_encoding_extensions; /* gag.  used by mkcache.c. */
#endif

struct save_as_data *
make_save_as_data( MWContext *context, bool allow_conversion_p,
		   int type, URL_Struct *url, const char *output_file )
{
  SaveAsDialog* saveasdialog = 0;
  const char *file_name = 0;
  struct save_as_data *sad;
  FILE *file = NULL;
  char *suggested_name = 0;
  bool use_dialog_p = false;
  bool exists_p = false;
  bool nuke_p = true;
  const char *address = url ? url->address : 0;
  const char *content_type = url ? url->content_type : 0;
  const char *content_encoding = url ? url->content_encoding : 0;
  const char *title;
  char buf [255];

  if (url)
    {
#ifdef MOZ_MAIL_NEWS
      if (!url->content_name)
		url->content_name = MimeGuessURLContentName(context, url->address);
#endif
      if (url->content_name)
		address = url->content_name;
    }

  if (output_file)
     file_name = output_file;
  else
  {
     bool really_allow_conversion_p = allow_conversion_p;
     if (content_type &&
         (!strcasecomp (content_type, UNKNOWN_CONTENT_TYPE) || 
          !strcasecomp (content_type, INTERNAL_PARSER)))
		content_type = 0;
     
      if (content_type && !*content_type) content_type = 0;
      if (content_encoding && !*content_encoding) content_encoding = 0;
      if (content_type && content_encoding)
         PR_snprintf (buf, sizeof (buf), XP_GetString(QTFE_SAVE_AS_TYPE_ENCODING),
                      content_type, content_encoding);
      else if (content_type)
         PR_snprintf (buf, sizeof (buf),
                      XP_GetString(QTFE_SAVE_AS_TYPE), content_type);
      else if (content_encoding)
         PR_snprintf (buf, sizeof (buf),
					 XP_GetString(QTFE_SAVE_AS_ENCODING), content_encoding);
      else
         PR_snprintf (buf, sizeof (buf), XP_GetString(QTFE_SAVE_AS));
      title = buf;
      
#if defined(XP_UNIX)
      if (fe_encoding_extensions)
      {
         int L = strlen (address);
         int i = 0;
         while (fe_encoding_extensions [i])
         {
            int L2 = strlen (fe_encoding_extensions [i]);
            if (L2 < L &&
                !strncmp (address + L - L2,
							fe_encoding_extensions [i],
                          L2))
            {
               
               /* The URL ends in ".Z" or ".gz" (or whatever.)
                  Take off that extension, and ask netlib what the type
                  of the resulting file is - if it is text/html or
                  text/plain (meaning it ends in ".html" or ".htm" or
                  ".txt" or god-knows-what-else) then (and only then)
                  strip off the .Z or .gz.  That we need to check the
                  extension like that is a kludge, but since we haven't
                  opened the URL yet, we don't have a content-type to
                  know for sure.  And it's just for the default file name
                  anyway...
               */
#ifdef NEW_DECODERS
               NET_cinfo *cinfo;
               
				  /* If the file ends in .html.Z, the suggested file
                                     name is .html, but we will allow it to be saved
                                     as .txt (uncompressed.)
                                     
                                     If the file ends in .ps.Z, the suggested file name
                                     is .ps.Z, and we will ignore a selection which says
                                     to save it as text. */
               really_allow_conversion_p = false;
               
               suggested_name = strdup (address);
               suggested_name [L - L2] = 0;
               /* don't free cinfo. */
               cinfo = NET_cinfo_find_type (suggested_name);
               if (cinfo &&
                   cinfo->type &&
                   (!strcasecomp (cinfo->type, TEXT_PLAIN) ||
                    !strcasecomp (cinfo->type, TEXT_HTML) ||
                    !strcasecomp (cinfo->type, TEXT_MDL) ||
                    /* always treat unknown content types as text/plain */
                    !strcasecomp (cinfo->type, UNKNOWN_CONTENT_TYPE)))
               {
                  /* then that's ok */
                  really_allow_conversion_p = allow_conversion_p;
                  break;
               }
               
               /* otherwise, continue. */
               free (suggested_name);
               suggested_name = 0;
#else  /* !NEW_DECODERS */
               suggested_name = strdup (address);
               suggested_name [L - L2] = 0;
               break;
#endif /* !NEW_DECODERS */
            }
            i++;
         }
      }
      
#endif // XP_UNIX
      
      saveasdialog = CONTEXT_DATA( context )->dialogPool()->saveasdialog;
      
    const char *def;
    const char* default_url = suggested_name ? suggested_name : address;
    if (! default_url)
       def = 0;
    else if ((def = strrchr (default_url, '/')))
       def++;
    else
       def = default_url;
    
    if( !saveasdialog )
       // ugleh a c k, will be better when this function has become a method of QtContext
       saveasdialog = new SaveAsDialog( CONTEXT_DATA( context ), 
                                        CONTEXT_DATA( context )->contentWidget() /* should be browserWidget */ );
    fprintf( stderr, "Setting as suggested name: %s\n", def );
    saveasdialog->setSelection( def ); 
    int res = saveasdialog->exec();
    if( res != QDialog::Accepted )
       return 0;
    
    if (suggested_name)
       free ((char *) suggested_name);
    
    use_dialog_p = true;
    file_name = saveasdialog->selectedFile();
    type      = saveasdialog->type();
  }
  
  
#ifdef EDITOR 
  if (context->type == MWContextEditor) {
	// NOTE:  let's check to make sure we can write into this file/directory
	//        before we start nuking data...
	//
	if ( !QTFE_StatReadWrite( file_name, true, false) ) { 
	  // NOTE:  we can't write into this directory or file... [ PUNT ]
	  //
	  char buf [1024];
	  PR_snprintf (buf, sizeof (buf),
				   XP_GetString(QTFE_ERROR_READ_ONLY), file_name);
	  CONTEXT_DATA( context )->perror( buf );
		  
	  return 0;
	}
  }
#endif
  
  /* If the file exists, confirm overwriting it. */
  {
    XP_StatStruct st;
    if (!stat (file_name, &st))
      {
		char *bp;
		int size;

		exists_p = true;

		size = XP_STRLEN (file_name) + 1;
		// #warning Internationalize this (do not have a QObject). Kalle.
		size += XP_STRLEN ( "Overwrite existing file %s?" ) + 1;
		bp = (char *) XP_ALLOC (size);
		if (!bp) return 0;
		// #warning Internationalize this (do not have a QObject). Kalle.
		PR_snprintf (bp, size, "Overwrite existing file %s?", file_name);
		if (!FE_Confirm (context, bp))
		  {
			XP_FREE (bp);
			return 0;
		  }
		XP_FREE (bp);
      }
  }

#ifdef EDITOR 
  if (exists_p && (context->type == MWContextEditor)) {
	// NOTE:  don't open the file or we'll destroy the ability to 
	//        create the backup file in EDT...
	//
	nuke_p = false;
  }
#endif

  if (nuke_p) {
	file = fopen (file_name, "w");
  }

  if (!file && nuke_p)
    {
      char buf [1024];
      PR_snprintf (buf, sizeof (buf),
				   XP_GetString(QTFE_ERROR_OPENING_FILE), file_name);
      CONTEXT_DATA( context )->perror( buf );
      sad = 0;
    }
  else
    {
      sad = (struct save_as_data *) malloc (sizeof (struct save_as_data));
      sad->context = context;
      sad->name = (char *) file_name;
      sad->file = file;
      sad->type = type;
      sad->done = NULL;
      sad->insert_base_tag = FALSE;
      sad->use_dialog_p = use_dialog_p;
      sad->content_length = 0;
      sad->bytes_read = 0;
      sad->url = url;
    }
  return sad;
}


int
fe_save_as_stream_write_method (NET_StreamClass *stream, const char *str, int32 len)
{
  struct save_as_data *sad = (struct save_as_data *) stream->data_object;
  fwrite ((char *) str, 1, len, sad->file);

  sad->bytes_read += len;

  if (sad->content_length > 0)
    FE_SetProgressBarPercent (sad->context,
			      (sad->bytes_read * 100) /
			      sad->content_length);
#if 0
  /* Oops, this makes the size twice as large as it should be. */
  FE_GraphProgress (sad->context, 0 /* #### url_s */,
		    sad->bytes_read, len, sad->content_length);
#endif
  return 1;
}

unsigned int
fe_save_as_stream_write_ready_method (NET_StreamClass* /*stream*/ ) 
{
  return(MAX_WRITE_READY);
}

void
fe_save_as_stream_complete_method (NET_StreamClass *stream)
{
  struct save_as_data *sad = (struct save_as_data *) stream->data_object;
  fclose (sad->file);
  if (sad->done)
    (*sad->done)(sad);
  sad->file = 0;
  if (sad->name)
    {
      free (sad->name);
      sad->name = 0;
    }
  if (sad->use_dialog_p)
    fe_LowerSynchronousURLDialog (sad->context);

  FE_GraphProgressDestroy (sad->context, 0 /* #### url */,
			   sad->content_length, sad->bytes_read);
  NET_RemoveURLFromCache(sad->url); /* cleanup */
  NET_RemoveDiskCacheObjects(0); /* kick the cache to notice the cleanup */
  free (sad);
  return;
}

void
fe_save_as_stream_abort_method (NET_StreamClass *stream, int /*status*/)
{
  struct save_as_data *sad = (struct save_as_data *) stream->data_object;
  fclose (sad->file);
  sad->file = 0;
  if (sad->name)
    {
      if (!unlink (sad->name))
	{
#if 0
	  char buf [1024];
	  PR_snprintf (buf, sizeof (buf),
			XP_GetString(XFE_ERROR_DELETING_FILE), sad->name);
	  fe_perror (sad->context, buf);
#endif
	}
      free (sad->name);
      sad->name = 0;
    }
  if (sad->use_dialog_p)
    fe_LowerSynchronousURLDialog (sad->context);

  FE_GraphProgressDestroy (sad->context, 0 /* #### url */,
			   sad->content_length, sad->bytes_read);
  /* We do *not* remove the URL from the cache at this point. */
  free (sad);
}


void fe_save_as_nastiness( QtContext *context, URL_Struct *url,
			   struct save_as_data *sad, int synchronous )
{
  SHIST_SavedData saved_data;
  assert (sad);
  if (! sad) return;

  /* Make sure layout saves the current state of form elements. */
  LO_SaveFormData( context->mwContext() );

  /* Hold on to the saved data. */
  XP_MEMCPY(&saved_data, &url->savedData, sizeof(SHIST_SavedData));

  /* make damn sure the form_data slot is zero'd or else all
   * hell will break loose
   */
  XP_MEMSET (&url->savedData, 0, sizeof (SHIST_SavedData));

  switch (sad->type)
    {
    case SaveAsDialog::Text:
      {
	PrintSetup p;
	fe_RaiseSynchronousURLDialog ( context->mwContext(), 
				       context->topLevelWidget(),
				      "saving");
	XL_InitializeTextSetup (&p);
	p.out = sad->file;
	p.filename = sad->name;
	p.url = url;
	free(sad);
	p.completion = fe_save_as_complete;
	p.carg = context->mwContext();
	XL_TranslateText (context->mwContext(), url, &p);
	fe_AwaitSynchronousURL (context->mwContext());
	break;
      }
    case SaveAsDialog::PostScript:
      {
	PrintSetup p;
	fe_RaiseSynchronousURLDialog (context->mwContext(), 
				      context->topLevelWidget(),
				      "saving");
	XL_InitializePrintSetup (&p);
	p.out = sad->file;
	p.filename = sad->name;
	p.url = url;
	free (sad);
	p.completion = fe_save_as_complete;
	p.carg = context->mwContext();
	XL_TranslatePostscript (context->mwContext(), url, &saved_data, &p);
	fe_AwaitSynchronousURL (context->mwContext());
	break;
      }

    case SaveAsDialog::Source:
      {
        if (!synchronous)
            context->getSecondaryURL (url, FO_CACHE_AND_SAVE_AS, sad, FALSE);
        else
            context->getSynchronousURL( context->topLevelWidget(),
					"saving",
					url,
					FO_CACHE_AND_SAVE_AS,
					(void *)sad);
	break;
      }
#ifdef SAVE_ALL
    case SaveAsDialog::HTMLAndImages:
      {
	char *addr = strdup (url->address);
	char *filename = sad->name;
	FILE *file = sad->file;
	NET_FreeURLStruct (url);
	free (sad);
	SAVE_SaveTree (context->mwContext(), addr, filename, file);
	break;
      }
#endif /* SAVE_ALL */
    default:
      abort ();
    }
}
