/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* 
 * xfe_dns.c --- hooking X Mozilla into the portable nonblocking DNS code.
 * Created: Jamie Zawinski <jwz@netscape.com>, 24-Dec-96.
 */


#include "unix-dns.h"
#include "xfe-dns.h"

/* Comment this out to turn off asynchronous dns lookup. */
/* #define UNIX_ASYNC_DNS */


extern int MK_OUT_OF_MEMORY;

static int dns_socket = -1;
static XtInputId dns_xt_id = 0;

#ifdef PROC1_DEBUG_PRINT
# define LOG(PREFIX,SMPRINTF_ARGS) \
    do { \
      char *s; \
      fprintf(stderr, "\tproc1 (%d): %s: ", getpid(), PREFIX); \
      s = PR_smprintf SMPRINTF_ARGS; \
      fprintf(stderr, "%s", s); \
      free(s); \
    } while(0)
#else  /* !PROC1_DEBUG_PRINT */
# define LOG(PREFIX,SMPRINTF_ARGS) do {} while(0)
#endif /* !PROC1_DEBUG_PRINT */


void
XFE_InitDNS_Early(int argc, char **argv)
{
int sock;
#ifdef UNIX_ASYNC_DNS
  static XP_Bool done = FALSE;

  LOG("XFE_InitDNS_Early", ("calling DNS_SpawnProcess.\n"));
  sock = DNS_SpawnProcess(argc, argv);

  XP_ASSERT(!done);
  if (done) return;
  done = TRUE;

  dns_socket = sock;
  if (dns_socket <= 0)
    {
      fprintf(stderr,
	      "%s: dns helper process initialization failed:\n"
	      "\thost name lookups will be synchronous (blocking.)\n",
	      argv[0]);
      /* #### better error message? */
      dns_socket = -1;
    }
#else  /*! UNIX_ASYNC_DNS */
  /* If the process never gets spawned, and sock never gets set, then
     the rest of this code will be a no-op (and FE_StartAsyncDNSLookup()
     will call gethostbyname() in the foreground.)
   */
  sock = -1;
#endif /*! UNIX_ASYNC_DNS */
}

static void
dns_xt_callback (void *closure, int *source, XtInputId *id)
{
  int status = 0;
  XP_ASSERT(*id == dns_xt_id);
  XP_ASSERT(*source == dns_socket);

  LOG("dns_xt_callback", ("calling DNS_ServiceProcess(fd=%d)\n", *source));

  status = DNS_ServiceProcess(*source);

  if (status < 0)
    {
      LOG("dns_xt_callback",
	  ("DNS_ServiceProcess returned %d -- shutting down\n", status));
      XtRemoveInput(dns_xt_id);
      dns_xt_id = 0;
      close(dns_socket);
      dns_socket = -1;
    }
}


void
XFE_InitDNS_Late(XtAppContext app)
{
  static XP_Bool done = FALSE;
  XP_ASSERT(!done);
  if (done) return;
  done = TRUE;

  if (dns_socket <= 0) return;

  dns_xt_id = XtAppAddInput (app, dns_socket,
			    (XtPointer) (XtInputReadMask|XtInputExceptMask),
			    dns_xt_callback, 0);
  XP_ASSERT(dns_xt_id);
  if (!dns_xt_id) return;

  LOG("XFE_InitDNS_Late", ("added input to Xt.\n"));
}


typedef struct x_pending_dns_lookup {
  void *id;
  char *name;
  int socket;

  XP_Bool done;
  int status;
  unsigned char result[4];

  struct x_pending_dns_lookup *next, *prev;
} x_pending_dns_lookup;

static x_pending_dns_lookup *queue = 0;


#ifdef PROC1_DEBUG_PRINT
static int
x_pending_dns_queue_length(void)
{
  int i;
  x_pending_dns_lookup *o;
  for (i = 0, o = queue; o; o = o->next)
    i++;
  return i;
}
#endif /* PROC1_DEBUG_PRINT */


static x_pending_dns_lookup *
new_lookup (const char *name, int socket)
{
  x_pending_dns_lookup *obj;
  XP_ASSERT(name);
  if (!name) return 0;

  XP_ASSERT(socket > 0);

  obj = (x_pending_dns_lookup *) malloc(sizeof(*obj));
  if (!obj) return 0;
  memset(obj, 0, sizeof(*obj));
  obj->name = strdup(name);
  obj->done = FALSE;
  obj->socket = socket;

  XP_ASSERT(!queue || queue->prev == 0);
  obj->next = queue;
  if (queue)
    queue->prev = obj;
  queue = obj;

  LOG("XFE new_lookup", ("linked in \"%s\", sock %d; ql=%d\n",
			 name, socket, x_pending_dns_queue_length()));

  return obj;
}

static int
free_lookup (x_pending_dns_lookup *obj)
{
  XP_ASSERT(obj);
  if (!obj) return -1;

  if (obj->prev)
    {
      XP_ASSERT(obj->prev->next != obj->next);
      obj->prev->next = obj->next;
    }
  if (obj->next)
    {
      XP_ASSERT(obj->next->prev != obj->prev);
      obj->next->prev = obj->prev;
    }
  if (queue == obj)
    queue = obj->next;

  LOG("XFE free_lookup", ("unlinked \"%s\", sock %d, id %d; ql=%d\n",
			  (obj->name ? obj->name : "(null)"),
			  obj->socket, (long) obj->id,
			  x_pending_dns_queue_length()));

  if (obj->name) free (obj->name);
  free(obj);
  return 0;
}


static int
xfe_dns_done_cb (void *id, void *closure,
		 int status, const char *result)
{
  /* status > 0 means "success"
     status == 0 means "aborted"
     status < 0 means "error".
   */

  x_pending_dns_lookup *obj = (x_pending_dns_lookup *) closure;
  XP_ASSERT(obj->id == id);

  LOG("XFE dns_done_cb (Bug #87736)", ("status = %d, result = \"%s\"\n",
                                       status, result));

  obj->done = TRUE;
  obj->status = status;
  if (status > 0)
    {
      unsigned char *ip = (unsigned char *) result;
      obj->result[0] = ip[0];
      obj->result[1] = ip[1];
      obj->result[2] = ip[2];
      obj->result[3] = ip[3];
    }

  XP_ASSERT(obj->socket > 0);


  LOG("XFE dns_done_cb",
      ("done with \"%s\"\n\t\t\tsock %d; id %d; ip=%d.%d.%d.%d; ql=%d\n",
       obj->name, obj->socket, (long) obj->id,
       obj->result[0], obj->result[1],
       obj->result[2], obj->result[3],
       x_pending_dns_queue_length()));


  /* This tells netlib that we're ready, and it should sink back down into
     whatever state machine is on this socket, and cause it to re-call
     FE_StartAsyncDNSLookup().  Since the object is now marked as `done',
     that will return a hostent, and then unlink and free the object.

     (Don't call it if status == 0, because that means that we've been
     called underneath FE_ClearDNSSelect/DNS_AbortHostLookup, and are
     thus already in netlib.)
   */
#ifdef NSPR20_DISABLED
  if (status != 0)
    NET_ProcessNet (obj->socket, NET_SOCKET_FD);
#endif 

  return 0;
}


static struct hostent *
make_hostent(const char *name, const unsigned char ip[4])
{
  static struct hostent reused_hostent = { 0, };
  static char *reused_addr_list[2] = { 0, };
  static char reused_addr[5] = { 0, };

  reused_hostent.h_name = (char *) name;
  reused_hostent.h_aliases = 0;
  reused_hostent.h_addrtype = AF_INET;
  reused_hostent.h_length = 4;
  reused_hostent.h_addr_list = reused_addr_list;
  reused_hostent.h_addr_list[0] = reused_addr;
  reused_hostent.h_addr_list[0][0] = ip[0];
  reused_hostent.h_addr_list[0][1] = ip[1];
  reused_hostent.h_addr_list[0][2] = ip[2];
  reused_hostent.h_addr_list[0][3] = ip[3];
  reused_hostent.h_addr_list[1] = 0;

  return &reused_hostent;
}

int
FE_StartAsyncDNSLookup(MWContext *context,
		       char *host_and_port,
		       void **hoststruct_ptr_ret,
		       int socket)
{
  int status = 0;
  x_pending_dns_lookup *obj;
  char *host, *c;

  *hoststruct_ptr_ret = 0;
  XP_ASSERT(host_and_port);
  if (!host_and_port || !*host_and_port)
    return -1;

  LOG("FE_StartAsyncDNSLookup (1)",
      ("\"%s\", sock %d; ql=%d\n", host_and_port, socket,
       x_pending_dns_queue_length()));

  XP_ASSERT(host_and_port);
  if (!host_and_port) return MK_OUT_OF_MEMORY;

  c = strchr(host_and_port, '@');
  if (c) host_and_port = c + 1;

  host = strdup(host_and_port);
  if (!host) return MK_OUT_OF_MEMORY;

  c = strchr(host, ':');
  if (c) *c = 0;


  if (dns_socket < 0)	/* socket got hosed */
    {
      LOG("FE_StartAsyncDNSLookup (1.5)", ("calling gethostbyname()\n"));
      *hoststruct_ptr_ret = gethostbyname(host);
      if (*hoststruct_ptr_ret)
	status = 0;
      else
	status = -1;
      goto DONE;
    }


  /* First look through the queue and see if we have an answer for this one.
   */
RESTART_SEARCH:
  for (obj = queue; obj; obj = obj->next)
    {

      LOG("  FE_StartAsyncDNSLookup (2)",
	  ("comparing %s/%d to %s/%d/%d\n", host, socket,
	   (obj->name ? obj->name : "(null)"), obj->socket, (long) obj->id));

      XP_ASSERT(obj->name);
      if (!obj->name) continue;
      if (socket == obj->socket)
	{

	  /* Since we're on the same socket, it had better be the
	     same host!  If not, netlib has broken the contract...
	     (Unfortunately, this seems to happen...  Treat it as
	     an implied abort?)
	   */
	  if (!!strcasecomp(host, obj->name))
	    {
	      LOG("FE_StartAsyncDNSLookup (2.5)", ("netlib is crazy!!\n"));
	      /* XP_ASSERT(0); */
	      free_lookup(obj);
	      obj = 0;
	      goto RESTART_SEARCH; /* since free_ changed the list links */
	    }

	  if (!obj->done)
	    {
	      /* A lookup has been started for this host already, and hasn't
		 come back.  (How do we get into this state?  (We do.)  It
		 means that FE_StartAsyncDNSLookup() was called twice with the
		 same host/sock, but before a response had come back on that
		 sock.)  So there will end up having been 3 calls.
	       */
	      LOG("FE_StartAsyncDNSLookup (3)",
		  ("\n\t\t\t already MK_WAITING_FOR_LOOKUP for %s\n", host));
	      status = MK_WAITING_FOR_LOOKUP;
	      goto DONE;
	    }
	  else if (obj->status <= 0)
	    {
	      /* Got an error looking up this host (that's bad!)
		 Remove this one from the list (is that safe?  I
		 *think* so...)
	       */

	      LOG("FE_StartAsyncDNSLookup (4): negative status",
		  ("%s\n", host));

	      status = obj->status;
	      free_lookup(obj);
	      obj = 0;
	      goto DONE;
	    }
	  else
	    {
	      /* Got an answer; construct a hostent and return it.
		 The caller expects this to be statically allocated
		 (same semantics as gethostbyname())
	       */
	      LOG("FE_StartAsyncDNSLookup (5): making hostent", ("%s\n",host));
	      *hoststruct_ptr_ret = make_hostent(obj->name, obj->result);
	      free_lookup(obj);
	      obj = 0;
	      status = 0;
	      goto DONE;
	    }
	}
    }
  /* Didn't find an entry for this host, so make one... */

  LOG("FE_StartAsyncDNSLookup (6): launching lookup (1)", ("%s\n", host));

  obj = new_lookup (host, socket);
  if (!obj)
    {
      status = MK_OUT_OF_MEMORY;
      goto DONE;
    }

  XP_ASSERT(!obj->done);
  status = DNS_AsyncLookupHost(host, xfe_dns_done_cb, obj, &obj->id);
  if (status >= 0)
    {
      status = MK_WAITING_FOR_LOOKUP;
    }
  else
    {
      free_lookup(obj);
      obj = 0;
      goto DONE;
    }


  LOG("FE_StartAsyncDNSLookup (7): launched lookup (2)",
      ("%s; id=%d\n", host, (long) obj->id));


  if (obj->done)
    {
      LOG("FE_StartAsyncDNSLookup (8): done already",
	  ("%s; sock=%d; id=%d\n", host, socket, (long) obj->id));

      status = obj->status;
      if (status >= 0)
	{
	  LOG("FE_StartAsyncDNSLookup (9): making hostent", ("%s\n", host));
	  *hoststruct_ptr_ret = make_hostent(obj->name, obj->result);
	}
      free_lookup(obj);
      obj = 0;
    }

 DONE:

  LOG("FE_StartAsyncDNSLookup (10)",
      ("\n\t\t\treturning %s for %s; ql=%d\n",
       (status == MK_WAITING_FOR_LOOKUP
	? "MK_WAITING_FOR_LOOKUP"
	: (status == 0 ? "0" :
	   status < 0 ? "negative" : "positive")),
       (host ? host : "(null)"),
       x_pending_dns_queue_length()));

  if (host) free(host);

  return status;
}


void
FE_ClearDNSSelect (MWContext *context, int socket)
{
  int status;
  x_pending_dns_lookup *obj;

  LOG("FE_ClearDNSSelect (1)", ("sock %d; ql=%d\n", socket,
				x_pending_dns_queue_length()));

  for (obj = queue; obj; obj = obj->next)
    {
      LOG("  FE_ClearDNSSelect (2)",
	  ("comparing %d to %s/%d/%d\n", socket,
	   (obj->name ? obj->name : "(null)"), obj->socket, (long) obj->id));

      XP_ASSERT(obj->name);
      if (!obj->name) continue;
      if (socket == obj->socket && !obj->done)
	{

	  LOG("FE_ClearDNSSelect (3)", ("calling DNS_AbortHostLookup\n"));

	  XP_ASSERT(!obj->done);
	  status = DNS_AbortHostLookup(obj->id);
	  return;
	}
    }

  LOG("FE_ClearDNSSelect (4)", ("no match for sock %d\n", socket));

  return;
}
