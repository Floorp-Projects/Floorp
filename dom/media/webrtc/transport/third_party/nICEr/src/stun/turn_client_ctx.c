/*
  Copyright (c) 2007, Adobe Systems, Incorporated
  Copyright (c) 2013, Mozilla

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  * Neither the name of Adobe Systems, Network Resonance, Mozilla nor
  the names of its contributors may be used to endorse or promote
  products derived from this software without specific prior written
  permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef USE_TURN

#include <assert.h>
#include <string.h>

#include "nr_api.h"
#include "r_time.h"
#include "async_timer.h"
#include "nr_socket_buffered_stun.h"
#include "stun.h"
#include "turn_client_ctx.h"
#include "ice_ctx.h"

int NR_LOG_TURN = 0;

#define TURN_MAX_PENDING_BYTES 32000

#define TURN_RTO 100  /* Hardcoded RTO estimate */
#define TURN_LIFETIME_REQUEST_SECONDS    3600  /* One hour */
#define TURN_USECS_PER_S                 1000000
#define TURN_REFRESH_SLACK_SECONDS       10    /* How long before expiry to refresh
                                                  allocations/permissions. The RFC 5766
                                                  Section 7 recommendation if 60 seconds,
                                                  but this is silly since the transaction
                                                  times out after about 5. */
#define TURN_PERMISSION_LIFETIME_SECONDS 300   /* 5 minutes. From RFC 5766 2.3 */

// Set to enable a temporary fix that will run the TURN reservation keep-alive
// logic when data is received via a TURN relayed path: a DATA_INDICATION packet is received.
// TODO(pkerr@mozilla.com) This should be replace/removed when bug 935806 is implemented.
#define REFRESH_RESERVATION_ON_RECV 1

static int nr_turn_stun_ctx_create(nr_turn_client_ctx *tctx, int type,
                                   NR_async_cb success_cb,
                                   NR_async_cb failure_cb,
                                   nr_turn_stun_ctx **ctxp);
static int nr_turn_stun_ctx_destroy(nr_turn_stun_ctx **ctxp);
static void nr_turn_stun_ctx_cb(NR_SOCKET s, int how, void *arg);
static int nr_turn_stun_set_auth_params(nr_turn_stun_ctx *ctx,
                                        char *realm, char *nonce);
static void nr_turn_client_refresh_timer_cb(NR_SOCKET s, int how, void *arg);
static int nr_turn_client_refresh_setup(nr_turn_client_ctx *ctx,
                                        nr_turn_stun_ctx **sctx);
static int nr_turn_client_start_refresh_timer(nr_turn_client_ctx *ctx,
                                              nr_turn_stun_ctx *sctx,
                                              UINT4 lifetime);
static int nr_turn_permission_create(nr_turn_client_ctx *ctx,
                                     const nr_transport_addr *addr,
                                     nr_turn_permission **permp);
static int nr_turn_permission_find(nr_turn_client_ctx *ctx,
                                   const nr_transport_addr *addr,
                                   nr_turn_permission **permp);
static int nr_turn_permission_destroy(nr_turn_permission **permp);
static void nr_turn_client_refresh_cb(NR_SOCKET s, int how, void *arg);
static void nr_turn_client_permissions_cb(NR_SOCKET s, int how, void *cb);
static int nr_turn_client_send_stun_request(nr_turn_client_ctx *ctx,
                                            nr_stun_message *req,
                                            int flags);

int nr_transport_addr_listnode_create(const nr_transport_addr *addr, nr_transport_addr_listnode **listnodep)
{
  nr_transport_addr_listnode *listnode = 0;
  int r,_status;

  if (!(listnode=RCALLOC(sizeof(nr_transport_addr_listnode)))) {
    ABORT(R_NO_MEMORY);
  }

  if ((r = nr_transport_addr_copy(&listnode->value, addr))) {
    ABORT(r);
  }

  *listnodep = listnode;
  listnode = 0;
  _status = 0;

abort:
  nr_transport_addr_listnode_destroy(&listnode);
  return(_status);
}

void nr_transport_addr_listnode_destroy(nr_transport_addr_listnode **listnode)
{
  RFREE(*listnode);
  *listnode = 0;
}

/* nr_turn_stun_ctx functions */
static int nr_turn_stun_ctx_create(nr_turn_client_ctx *tctx, int mode,
                                   NR_async_cb success_cb,
                                   NR_async_cb error_cb,
                                   nr_turn_stun_ctx **ctxp)
{
  nr_turn_stun_ctx *sctx = 0;
  int r,_status;
  char label[256];

  if (!(sctx=RCALLOC(sizeof(nr_turn_stun_ctx))))
    ABORT(R_NO_MEMORY);

  /* TODO(ekr@rtfm.com): label by phase */
  snprintf(label, sizeof(label), "%s:%s", tctx->label, ":TURN");

  if ((r=nr_stun_client_ctx_create(label, tctx->sock, &tctx->turn_server_addr,
                                   TURN_RTO, &sctx->stun))) {
    ABORT(r);
  }

  /* Set the STUN auth parameters, but don't set authentication on.
     For that we need the nonce, set in nr_turn_stun_set_auth_params.
  */
  sctx->stun->auth_params.username=tctx->username;
  INIT_DATA(sctx->stun->auth_params.password,
            tctx->password->data, tctx->password->len);

  sctx->tctx=tctx;
  sctx->success_cb=success_cb;
  sctx->error_cb=error_cb;
  sctx->mode=mode;
  sctx->last_error_code=0;
  STAILQ_INIT(&sctx->addresses_tried);

  /* Add ourselves to the tctx's list */
  STAILQ_INSERT_TAIL(&tctx->stun_ctxs, sctx, entry);
  *ctxp=sctx;

  _status=0;
abort:
  if (_status) {
    nr_turn_stun_ctx_destroy(&sctx);
  }
  return(_status);
}

/* Note: this function does not pull us off the tctx's list. */
static int nr_turn_stun_ctx_destroy(nr_turn_stun_ctx **ctxp)
{
  nr_turn_stun_ctx *ctx;

  if (!ctxp || !*ctxp)
    return 0;

  ctx = *ctxp;
  *ctxp = 0;

  nr_stun_client_ctx_destroy(&ctx->stun);
  RFREE(ctx->realm);
  RFREE(ctx->nonce);

  while (!STAILQ_EMPTY(&ctx->addresses_tried)) {
    nr_transport_addr_listnode *listnode = STAILQ_FIRST(&ctx->addresses_tried);
    STAILQ_REMOVE_HEAD(&ctx->addresses_tried, entry);
    nr_transport_addr_listnode_destroy(&listnode);
  }

  RFREE(ctx);

  return 0;
}

static int nr_turn_stun_set_auth_params(nr_turn_stun_ctx *ctx,
                                        char *realm, char *nonce)
{
  int _status;

  RFREE(ctx->realm);
  RFREE(ctx->nonce);

  assert(realm);
  if (!realm)
    ABORT(R_BAD_ARGS);
  ctx->realm=r_strdup(realm);
  if (!ctx->realm)
    ABORT(R_NO_MEMORY);

  assert(nonce);
  if (!nonce)
    ABORT(R_BAD_ARGS);
  ctx->nonce=r_strdup(nonce);
  if (!ctx->nonce)
    ABORT(R_NO_MEMORY);

  RFREE(ctx->stun->realm);
  ctx->stun->realm = r_strdup(ctx->realm);
  if (!ctx->stun->realm)
    ABORT(R_NO_MEMORY);

  ctx->stun->auth_params.realm = ctx->realm;
  ctx->stun->auth_params.nonce = ctx->nonce;
  ctx->stun->auth_params.authenticate = 1;  /* May already be 1 */

  _status=0;
abort:
  return(_status);
}

static int nr_turn_stun_ctx_start(nr_turn_stun_ctx *ctx)
{
  int r, _status;
  nr_turn_client_ctx *tctx = ctx->tctx;
  nr_transport_addr_listnode *address_tried = 0;

  if ((r=nr_stun_client_reset(ctx->stun))) {
    r_log(NR_LOG_TURN, LOG_ERR, "TURN(%s): Couldn't reset STUN",
          tctx->label);
    ABORT(r);
  }

  if ((r=nr_stun_client_start(ctx->stun, ctx->mode, nr_turn_stun_ctx_cb, ctx))) {
    r_log(NR_LOG_TURN, LOG_ERR, "TURN(%s): Couldn't start STUN",
          tctx->label);
    ABORT(r);
  }

  if ((r=nr_transport_addr_listnode_create(&ctx->stun->peer_addr, &address_tried))) {
    ABORT(r);
  }

  STAILQ_INSERT_TAIL(&ctx->addresses_tried, address_tried, entry);

  _status=0;
abort:
  return _status;
}

static int nr_turn_stun_ctx_handle_redirect(nr_turn_stun_ctx *ctx)
{
  int r, _status;
  nr_turn_client_ctx *tctx = ctx->tctx;
  nr_stun_message_attribute *ec;
  nr_stun_message_attribute *attr;
  nr_transport_addr *alternate_addr = 0;
  int index = 0;

  if (!tctx->ctx) {
    /* If we were to require TCP nr_sockets to allow multiple connect calls by
     * disconnecting and re-connecting, we could avoid this requirement. */
    r_log(NR_LOG_TURN, LOG_ERR,
          "TURN(%s): nr_turn_stun_ctx_handle_redirect is not supported when "
          "there is no ICE ctx, and by extension no socket factory, since we "
          "need that to create new sockets in the TCP case",
          tctx->label);
    ABORT(R_BAD_ARGS);
  }

  if (ctx->stun->response->header.type != NR_STUN_MSG_ALLOCATE_ERROR_RESPONSE) {
    r_log(NR_LOG_TURN, LOG_ERR,
          "TURN(%s): nr_turn_stun_ctx_handle_redirect called for something "
          "other than an Allocate error response (type=%d)",
          tctx->label, ctx->stun->response->header.type);
    ABORT(R_BAD_ARGS);
  }

  if (!nr_stun_message_has_attribute(ctx->stun->response,
                                     NR_STUN_ATTR_ERROR_CODE, &ec) ||
      (ec->u.error_code.number != 300)) {
    r_log(NR_LOG_TURN, LOG_ERR,
          "TURN(%s): nr_turn_stun_ctx_handle_redirect called without a "
          "300 response",
          tctx->label);
    ABORT(R_BAD_ARGS);
  }

  while (!alternate_addr && !nr_stun_message_get_attribute(
      ctx->stun->response, NR_STUN_ATTR_ALTERNATE_SERVER, index++, &attr)) {
    alternate_addr = &attr->u.alternate_server;

    // TODO: Someday we may need to handle IP version switching, but it is
    // unclear how that is supposed to work with ICE when the IP version of
    // the candidate's base address is fixed...
    if (alternate_addr->ip_version != tctx->turn_server_addr.ip_version) {
      r_log(NR_LOG_TURN, LOG_INFO,
            "TURN(%s): nr_turn_stun_ctx_handle_redirect not trying %s, since it is a different IP version",
            tctx->label, alternate_addr->as_string);
      alternate_addr = 0;
      continue;
    }

    /* Check if we've already tried this, and ignore if we have */
    nr_transport_addr_listnode *address_tried = 0;
    STAILQ_FOREACH(address_tried, &ctx->addresses_tried, entry) {
      /* Ignore protocol */
      alternate_addr->protocol = address_tried->value.protocol;
      if (!nr_transport_addr_cmp(alternate_addr, &address_tried->value, NR_TRANSPORT_ADDR_CMP_MODE_ALL)) {
        r_log(NR_LOG_TURN, LOG_INFO,
              "TURN(%s): nr_turn_stun_ctx_handle_redirect already tried %s, ignoring",
              tctx->label, alternate_addr->as_string);
        alternate_addr = 0;
        break;
      }
    }
  }

  if (!alternate_addr) {
    /* Should we use a different error code depending on why we didn't find
     * one? (eg; no ALTERNATE-SERVERS at all, none that we have not tried
     * already, none that are of a compatible IP version?) */
    r_log(NR_LOG_TURN, LOG_ERR,
          "TURN(%s): nr_turn_stun_ctx_handle_redirect did not find a viable "
          "ALTERNATE-SERVER",
          tctx->label);
    ABORT(R_FAILED);
  }

  r_log(NR_LOG_TURN, LOG_INFO,
      "TURN(%s): nr_turn_stun_ctx_handle_redirect trying %s",
      tctx->label, alternate_addr->as_string);

  /* This also handles the call to nr_transport_addr_fmt_addr_string */
  if ((r = nr_transport_addr_copy_addrport(&tctx->turn_server_addr,
                                           alternate_addr))) {
    r_log(NR_LOG_TURN, LOG_ERR,
          "TURN(%s): nr_turn_stun_ctx_handle_redirect copying ALTERNATE-SERVER "
          "failed(%d)!",
          tctx->label, r);
    assert(0);
    ABORT(r);
  }

  /* TURN server address is now updated. Restart the STUN Allocate ctx. Note
   * that we do not attempt to update the local address; if the TURN server
   * redirects to something that is not best reached from the already-selected
   * local address, oh well. */

  if (tctx->turn_server_addr.protocol == IPPROTO_TCP) {
    /* For TCP, we need to replace the underlying nr_socket, since we cannot
     * un-connect it from the old server. */
    /* If we were to require TCP nr_sockets to allow multiple connect calls by
     * disconnecting and re-connecting, we could avoid this stuff, and just
     * call nr_socket_connect. */
    nr_transport_addr old_local_addr;
    nr_socket* new_socket;
    if ((r = nr_socket_getaddr(tctx->sock, &old_local_addr))) {
      r_log(NR_LOG_TURN, LOG_ERR,
            "TURN(%s): nr_turn_stun_ctx_handle_redirect "
            "failed to get old local address (%d)!",
            tctx->label, r);
      assert(0);
      ABORT(r);
    }

    if ((r = nr_socket_factory_create_socket(tctx->ctx->socket_factory,
            &old_local_addr, &new_socket))) {
      r_log(NR_LOG_TURN, LOG_ERR,
            "TURN(%s): nr_turn_stun_ctx_handle_redirect "
            "failed to create new raw TCP socket for redirect (%d)!",
            tctx->label, r);
      assert(0);
      ABORT(r);
    }

    if ((r = nr_socket_buffered_stun_reset(tctx->sock, new_socket))) {
      /* nr_socket_buffered_stun_reset always takes ownership of |new_socket| */
      r_log(NR_LOG_TURN, LOG_ERR,
            "TURN(%s): nr_turn_stun_ctx_handle_redirect "
            "failed to update raw TCP socket (%d)!",
            tctx->label, r);
      assert(0);
      ABORT(r);
    }

    if ((r = nr_socket_connect(tctx->sock, &tctx->turn_server_addr))) {
      if (r != R_WOULDBLOCK) {
        r_log(NR_LOG_TURN, LOG_ERR,
              "TURN(%s): nr_turn_stun_ctx_handle_redirect nr_socket_connect "
              "failed(%d)!",
              tctx->label, r);
        assert(0);
        ABORT(r);
      }
    }
  }

  nr_transport_addr_copy(&ctx->stun->peer_addr, &tctx->turn_server_addr);

  if ((r = nr_turn_stun_ctx_start(ctx))) {
    r_log(NR_LOG_TURN, LOG_ERR,
          "TURN(%s): nr_turn_stun_ctx_handle_redirect nr_turn_stun_ctx_start "
          "failed(%d)!",
          tctx->label, r);
    assert(0);
    ABORT(r);
  }

  _status = 0;
abort:
  return _status;
}

static void nr_turn_stun_ctx_cb(NR_SOCKET s, int how, void *arg)
{
  int r, _status;
  nr_turn_stun_ctx *ctx = (nr_turn_stun_ctx *)arg;

  ctx->last_error_code = ctx->stun->error_code;

  switch (ctx->stun->state) {
    case NR_STUN_CLIENT_STATE_DONE:
      /* Save the realm and nonce */
      if (ctx->stun->realm && (!ctx->tctx->realm || strcmp(ctx->stun->realm,
                                                           ctx->tctx->realm))) {
        RFREE(ctx->tctx->realm);
        ctx->tctx->realm = r_strdup(ctx->stun->realm);
        if (!ctx->tctx->realm)
          ABORT(R_NO_MEMORY);
      }
      if (ctx->stun->nonce && (!ctx->tctx->nonce || strcmp(ctx->stun->nonce,
                                                           ctx->tctx->nonce))) {
        RFREE(ctx->tctx->nonce);
        ctx->tctx->nonce = r_strdup(ctx->stun->nonce);
        if (!ctx->tctx->nonce)
          ABORT(R_NO_MEMORY);
      }

      ctx->retry_ct=0;
      ctx->success_cb(0, 0, ctx);
      break;

    case NR_STUN_CLIENT_STATE_FAILED:
      /* Special case: if this is an authentication error,
         we retry once. This allows the 401/438 nonce retry
         paradigm. After that, we fail */
      /* TODO(ekr@rtfm.com): 401 needs a #define */
      /* TODO(ekr@rtfm.com): Add alternate-server (Mozilla bug 857688) */
      if (ctx->stun->error_code == 438) {
        // track 438s for ice telemetry
        nr_accumulate_count(&(ctx->tctx->cnt_438s), 1);
      }
      if (ctx->stun->error_code == 401 || ctx->stun->error_code == 438) {
        if (ctx->retry_ct > 0) {
          if (ctx->stun->error_code == 401) {
            // track 401s for ice telemetry
            nr_accumulate_count(&(ctx->tctx->cnt_401s), 1);
          }
          r_log(NR_LOG_TURN, LOG_WARNING, "TURN(%s): Exceeded the number of retries", ctx->tctx->label);
          ABORT(R_FAILED);
        }

        if (!ctx->stun->nonce) {
          r_log(NR_LOG_TURN, LOG_WARNING, "TURN(%s): 401 but no nonce", ctx->tctx->label);
          ABORT(R_FAILED);
        }
        if (!ctx->stun->realm) {
          r_log(NR_LOG_TURN, LOG_WARNING, "TURN(%s): 401 but no realm", ctx->tctx->label);
          ABORT(R_FAILED);
        }

        /* Try to retry */
        if ((r=nr_turn_stun_set_auth_params(ctx, ctx->stun->realm,
                                            ctx->stun->nonce)))
          ABORT(r);

        ctx->stun->error_code = 0;  /* Reset to avoid inf-looping */

        if ((r=nr_turn_stun_ctx_start(ctx))) {
          r_log(NR_LOG_TURN, LOG_ERR, "TURN(%s): Couldn't start STUN", ctx->tctx->label);
          ABORT(r);
        }

        ctx->retry_ct++;
      } else if (ctx->stun->error_code == 300) {
        r_log(NR_LOG_TURN, LOG_INFO,
              "TURN(%s): Redirect received, restarting TURN", ctx->tctx->label);
        /* We don't limit this with a retry counter, we limit redirects by
         * checking whether we've tried the ALTERNATE-SERVER yet. */
        ctx->retry_ct = 0;
        if ((r = nr_turn_stun_ctx_handle_redirect(ctx))) {
          ABORT(r);
        }
      } else {
        ABORT(R_FAILED);
      }
      break;

    case NR_STUN_CLIENT_STATE_TIMED_OUT:
      ABORT(R_FAILED);
      break;

    case NR_STUN_CLIENT_STATE_CANCELLED:
      assert(0);  /* Shouldn't happen */
      return;
      break;

    default:
      assert(0);  /* Shouldn't happen */
      return;
  }

  _status=0;
abort:
  if (_status) {
    ctx->error_cb(0, 0, ctx);
  }
}

/* nr_turn_client_ctx functions */
int nr_turn_client_ctx_create(const char* label, nr_socket* sock,
                              const char* username, Data* password,
                              nr_transport_addr* addr, nr_ice_ctx* ice_ctx,
                              nr_turn_client_ctx** ctxp) {
  nr_turn_client_ctx *ctx=0;
  int r,_status;

  if ((r=r_log_register("turn", &NR_LOG_TURN)))
    ABORT(r);

  if(!(ctx=RCALLOC(sizeof(nr_turn_client_ctx))))
    ABORT(R_NO_MEMORY);

  STAILQ_INIT(&ctx->stun_ctxs);
  STAILQ_INIT(&ctx->permissions);

  if(!(ctx->label=r_strdup(label)))
    ABORT(R_NO_MEMORY);

  ctx->sock=sock;
  ctx->username = r_strdup(username);
  if (!ctx->username)
    ABORT(R_NO_MEMORY);

  if ((r=r_data_create(&ctx->password, password->data, password->len)))
    ABORT(r);
  if ((r=nr_transport_addr_copy(&ctx->turn_server_addr, addr)))
    ABORT(r);

  ctx->state = NR_TURN_CLIENT_STATE_INITTED;
  if (addr->protocol == IPPROTO_TCP) {
    if ((r=nr_socket_connect(ctx->sock, &ctx->turn_server_addr))) {
      if (r != R_WOULDBLOCK)
        ABORT(r);
    }
  }

  ctx->ctx = ice_ctx;

  *ctxp=ctx;

  _status=0;
abort:
  if(_status){
    nr_turn_client_ctx_destroy(&ctx);
  }
  return(_status);
}

int
nr_turn_client_ctx_destroy(nr_turn_client_ctx **ctxp)
{
  nr_turn_client_ctx *ctx;

  if(!ctxp || !*ctxp)
    return(0);

  ctx=*ctxp;
  *ctxp = 0;

  if (ctx->label)
    r_log(NR_LOG_TURN, LOG_DEBUG, "TURN(%s): destroy", ctx->label);

  nr_turn_client_deallocate(ctx);

  /* Cancel frees the rest of our data */
  RFREE(ctx->label);
  ctx->label = 0;

  nr_turn_client_cancel(ctx);

  RFREE(ctx->username);
  ctx->username = 0;
  r_data_destroy(&ctx->password);
  RFREE(ctx->nonce);
  ctx->nonce = 0;
  RFREE(ctx->realm);
  ctx->realm = 0;

  /* Destroy the STUN client ctxs */
  while (!STAILQ_EMPTY(&ctx->stun_ctxs)) {
    nr_turn_stun_ctx *stun = STAILQ_FIRST(&ctx->stun_ctxs);
    STAILQ_REMOVE_HEAD(&ctx->stun_ctxs, entry);
    nr_turn_stun_ctx_destroy(&stun);
  }

  /* Destroy the permissions */
  while (!STAILQ_EMPTY(&ctx->permissions)) {
    nr_turn_permission *perm = STAILQ_FIRST(&ctx->permissions);
    STAILQ_REMOVE_HEAD(&ctx->permissions, entry);
    nr_turn_permission_destroy(&perm);
  }

  RFREE(ctx);

  return(0);
}

int nr_turn_client_cancel(nr_turn_client_ctx *ctx)
{
  nr_turn_stun_ctx *stun = 0;

  if (ctx->state == NR_TURN_CLIENT_STATE_CANCELLED ||
      ctx->state == NR_TURN_CLIENT_STATE_FAILED)
    return(0);

  if (ctx->label)
    r_log(NR_LOG_TURN, LOG_INFO, "TURN(%s): cancelling", ctx->label);

  /* Cancel the STUN client ctxs */
  stun = STAILQ_FIRST(&ctx->stun_ctxs);
  while (stun) {
    nr_stun_client_cancel(stun->stun);
    stun = STAILQ_NEXT(stun, entry);
  }

  /* Cancel the timers, if not already cancelled */
  NR_async_timer_cancel(ctx->connected_timer_handle);
  NR_async_timer_cancel(ctx->refresh_timer_handle);

  ctx->state = NR_TURN_CLIENT_STATE_CANCELLED;

  return(0);
}

int nr_turn_client_send_stun_request(nr_turn_client_ctx *ctx,
                                     nr_stun_message *req,
                                     int flags)
{
  int r,_status;

  if ((r=nr_stun_encode_message(req)))
    ABORT(r);

  if ((r=nr_socket_sendto(ctx->sock,
                          req->buffer, req->length, flags,
                          &ctx->turn_server_addr))) {
    r_log(NR_LOG_TURN, LOG_WARNING, "TURN(%s): Failed sending request",
          ctx->label);
    ABORT(r);
  }

  _status=0;
abort:
  return(_status);
}

int nr_turn_client_deallocate(nr_turn_client_ctx *ctx)
{
  int r,_status;
  nr_stun_message *aloc = 0;
  nr_stun_client_auth_params auth;
  nr_stun_client_refresh_request_params refresh;

  if (ctx->state != NR_TURN_CLIENT_STATE_ALLOCATED)
    return(0);

  r_log(NR_LOG_TURN, LOG_INFO, "TURN(%s): deallocating", ctx->label);

  refresh.lifetime_secs = 0;

  auth.username = ctx->username;
  INIT_DATA(auth.password, ctx->password->data, ctx->password->len);

  auth.realm = ctx->realm;
  auth.nonce = ctx->nonce;

  auth.authenticate = 1;

  if ((r=nr_stun_build_refresh_request(&auth, &refresh, &aloc)))
    ABORT(r);

  // We are only sending a single request here because we are in the process of
  // shutting everything down. Theoretically we should probably start a seperate
  // STUN transaction which outlives the TURN context.
  if ((r=nr_turn_client_send_stun_request(ctx, aloc, 0)))
    ABORT(r);

  ctx->state = NR_TURN_CLIENT_STATE_DEALLOCATING;

  _status=0;
abort:
  nr_stun_message_destroy(&aloc);
  return(_status);
}

static void nr_turn_client_fire_finished_cb(nr_turn_client_ctx *ctx)
  {
    if (ctx->finished_cb) {
      NR_async_cb finished_cb=ctx->finished_cb;
      ctx->finished_cb=0;
      finished_cb(0, 0, ctx->cb_arg);
    }
  }

int nr_turn_client_failed(nr_turn_client_ctx *ctx)
{
  if (ctx->state == NR_TURN_CLIENT_STATE_FAILED ||
      ctx->state == NR_TURN_CLIENT_STATE_CANCELLED)
    return(0);

  r_log(NR_LOG_TURN, LOG_WARNING, "TURN(%s) failed", ctx->label);
  nr_turn_client_cancel(ctx);
  ctx->state = NR_TURN_CLIENT_STATE_FAILED;
  nr_turn_client_fire_finished_cb(ctx);

  return(0);
}

int nr_turn_client_get_relayed_address(nr_turn_client_ctx *ctx,
                                       nr_transport_addr *relayed_address)
{
  int r, _status;

  if (ctx->state != NR_TURN_CLIENT_STATE_ALLOCATED)
    ABORT(R_FAILED);

  if (r=nr_transport_addr_copy(relayed_address, &ctx->relay_addr))
    ABORT(r);

  _status=0;
abort:
  return(_status);
}

int nr_turn_client_get_mapped_address(nr_turn_client_ctx *ctx,
                                       nr_transport_addr *mapped_address)
{
  int r, _status;

  if (ctx->state != NR_TURN_CLIENT_STATE_ALLOCATED)
    ABORT(R_FAILED);

  if (r=nr_transport_addr_copy(mapped_address, &ctx->mapped_addr))
    ABORT(r);

  _status=0;
abort:
  return(_status);
}

static void nr_turn_client_allocate_cb(NR_SOCKET s, int how, void *arg)
{
  nr_turn_stun_ctx *ctx = (nr_turn_stun_ctx *)arg;
  nr_turn_stun_ctx *refresh_ctx;
  int r,_status;

  ctx->tctx->state = NR_TURN_CLIENT_STATE_ALLOCATED;

  if ((r=nr_transport_addr_copy(
          &ctx->tctx->relay_addr,
          &ctx->stun->results.allocate_response.relay_addr)))
    ABORT(r);

  if ((r=nr_transport_addr_copy(
          &ctx->tctx->mapped_addr,
          &ctx->stun->results.allocate_response.mapped_addr)))
    ABORT(r);

  if ((r=nr_turn_client_refresh_setup(ctx->tctx, &refresh_ctx)))
    ABORT(r);

  if ((r=nr_turn_client_start_refresh_timer(
          ctx->tctx, refresh_ctx,
          ctx->stun->results.allocate_response.lifetime_secs)))
    ABORT(r);

  r_log(NR_LOG_TURN, LOG_INFO,
        "TURN(%s): Succesfully allocated addr %s lifetime=%u",
        ctx->tctx->label,
        ctx->tctx->relay_addr.as_string,
        ctx->stun->results.allocate_response.lifetime_secs);

  nr_turn_client_fire_finished_cb(ctx->tctx);
  _status=0;
abort:
  if (_status) {
    nr_turn_client_failed(ctx->tctx);
  }
}

static void nr_turn_client_error_cb(NR_SOCKET s, int how, void *arg)
{
  nr_turn_stun_ctx *ctx = (nr_turn_stun_ctx *)arg;

  r_log(NR_LOG_TURN, LOG_WARNING, "TURN(%s): mode %d, %s",
        ctx->tctx->label, ctx->mode, __FUNCTION__);

  nr_turn_client_failed(ctx->tctx);
}

static void nr_turn_client_permission_error_cb(NR_SOCKET s, int how, void *arg)
{
  nr_turn_stun_ctx *ctx = (nr_turn_stun_ctx *)arg;

  if (ctx->last_error_code == 403) {
    // track 403s for ice telemetry
    nr_accumulate_count(&(ctx->tctx->cnt_403s), 1);
    r_log(NR_LOG_TURN, LOG_WARNING, "TURN(%s): mode %d, permission denied",
          ctx->tctx->label, ctx->mode);

  } else{
    nr_turn_client_error_cb(0, 0, ctx);
  }
}

int nr_turn_client_allocate(nr_turn_client_ctx *ctx,
                            NR_async_cb finished_cb, void *cb_arg)
{
  nr_turn_stun_ctx *stun = 0;
  int r,_status;

  if(ctx->state == NR_TURN_CLIENT_STATE_FAILED ||
     ctx->state == NR_TURN_CLIENT_STATE_CANCELLED){
    /* TURN TCP contexts can fail before we ever try to form an allocation,
     * since the TCP connection can fail. It is also conceivable that a TURN
     * TCP context could be cancelled before we are done forming all
     * allocations (although we do not do this at the time this code was
     * written) */
    assert(ctx->turn_server_addr.protocol == IPPROTO_TCP);
    ABORT(R_NOT_FOUND);
  }

  assert(ctx->state == NR_TURN_CLIENT_STATE_INITTED);

  ctx->finished_cb=finished_cb;
  ctx->cb_arg=cb_arg;

  if ((r=nr_turn_stun_ctx_create(ctx, NR_TURN_CLIENT_MODE_ALLOCATE_REQUEST,
                                 nr_turn_client_allocate_cb,
                                 nr_turn_client_error_cb,
                                 &stun)))
    ABORT(r);
  stun->stun->params.allocate_request.lifetime_secs =
      TURN_LIFETIME_REQUEST_SECONDS;

  if (ctx->state == NR_TURN_CLIENT_STATE_INITTED) {
      if ((r=nr_turn_stun_ctx_start(stun)))
        ABORT(r);
      ctx->state = NR_TURN_CLIENT_STATE_ALLOCATING;
  } else {
      ABORT(R_ALREADY);
  }

  _status=0;
abort:
  if (_status) {
    nr_turn_client_failed(ctx);
  }

  return(_status);
}

int nr_turn_client_process_response(nr_turn_client_ctx *ctx,
                                    UCHAR *msg, int len,
                                    nr_transport_addr *turn_server_addr)
{
  int r, _status;
  nr_turn_stun_ctx *sc1;

  switch (ctx->state) {
    case NR_TURN_CLIENT_STATE_ALLOCATING:
    case NR_TURN_CLIENT_STATE_ALLOCATED:
      break;
    default:
      ABORT(R_FAILED);
  }

  sc1 = STAILQ_FIRST(&ctx->stun_ctxs);
  while (sc1) {
    r = nr_stun_client_process_response(sc1->stun, msg, len, turn_server_addr);
    if (!r)
      break;
    if (r==R_RETRY)  /* Likely a 401 and we will retry */
      break;
    if (r != R_REJECTED)
      ABORT(r);
    sc1 = STAILQ_NEXT(sc1, entry);
  }
  if (!sc1)
    ABORT(R_REJECTED);

  _status=0;
abort:
  return(_status);
}

static int nr_turn_client_refresh_setup(nr_turn_client_ctx *ctx,
                                        nr_turn_stun_ctx **sctx)
{
  nr_turn_stun_ctx *stun = 0;
  int r,_status;

  assert(ctx->state == NR_TURN_CLIENT_STATE_ALLOCATED);
  if (ctx->state != NR_TURN_CLIENT_STATE_ALLOCATED)
    ABORT(R_NOT_PERMITTED);

  if ((r=nr_turn_stun_ctx_create(ctx, NR_TURN_CLIENT_MODE_REFRESH_REQUEST,
                                 nr_turn_client_refresh_cb,
                                 nr_turn_client_error_cb,
                                 &stun)))
    ABORT(r);

  if ((r=nr_turn_stun_set_auth_params(stun, ctx->realm, ctx->nonce)))
    ABORT(r);

  stun->stun->params.refresh_request.lifetime_secs =
      TURN_LIFETIME_REQUEST_SECONDS;


  *sctx=stun;

  _status=0;
abort:
  return(_status);
}

static int nr_turn_client_start_refresh_timer(nr_turn_client_ctx *tctx,
                                              nr_turn_stun_ctx *sctx,
                                              UINT4 lifetime)
{
  int _status;

  assert(!tctx->refresh_timer_handle);

  if (lifetime <= TURN_REFRESH_SLACK_SECONDS) {
    r_log(NR_LOG_TURN, LOG_ERR, "Too short lifetime specified for turn %u", lifetime);
    ABORT(R_BAD_DATA);
  }

  if (lifetime > 3600)
    lifetime = 3600;

  lifetime -= TURN_REFRESH_SLACK_SECONDS;

  r_log(NR_LOG_TURN, LOG_DEBUG, "TURN(%s): Setting refresh timer for %u seconds",
        tctx->label, lifetime);
  NR_ASYNC_TIMER_SET(lifetime * 1000, nr_turn_client_refresh_timer_cb, sctx,
                     &tctx->refresh_timer_handle);

  _status=0;
abort:
  if (_status) {
    nr_turn_client_failed(tctx);
  }
  return _status;
}

static void nr_turn_client_refresh_timer_cb(NR_SOCKET s, int how, void *arg)
{
  nr_turn_stun_ctx *ctx = (nr_turn_stun_ctx *)arg;
  int r,_status;

  r_log(NR_LOG_TURN, LOG_DEBUG, "TURN(%s): Refresh timer fired",
        ctx->tctx->label);

  ctx->tctx->refresh_timer_handle=0;
  if ((r=nr_turn_stun_ctx_start(ctx))) {
    ABORT(r);
  }

  _status=0;
abort:
  if (_status) {
    nr_turn_client_failed(ctx->tctx);
  }
  return;
}

static void nr_turn_client_refresh_cb(NR_SOCKET s, int how, void *arg)
{
  int r, _status;
  nr_turn_stun_ctx *ctx = (nr_turn_stun_ctx *)arg;
  /* Save lifetime from the reset */
  UINT4 lifetime = ctx->stun->results.refresh_response.lifetime_secs;

  r_log(NR_LOG_TURN, LOG_DEBUG, "TURN(%s): Refresh succeeded. lifetime=%u",
        ctx->tctx->label, lifetime);

  if ((r=nr_turn_client_start_refresh_timer(
          ctx->tctx, ctx, lifetime)))
    ABORT(r);

  _status=0;

abort:
  if (_status) {
    nr_turn_client_failed(ctx->tctx);
  }
}

/* TODO(ekr@rtfm.com): We currently don't support channels.
   We might in the future. Mozilla bug 857736 */
int nr_turn_client_send_indication(nr_turn_client_ctx *ctx,
                                   const UCHAR *msg, size_t len,
                                   int flags, const nr_transport_addr *remote_addr)
{
  int r,_status;
  nr_stun_client_send_indication_params params = { { 0 } };
  nr_stun_message *ind = 0;

  if (ctx->state != NR_TURN_CLIENT_STATE_ALLOCATED)
    ABORT(R_FAILED);

  r_log(NR_LOG_TURN, LOG_DEBUG, "TURN(%s): Send indication len=%zu",
        ctx->label, len);

  if ((r=nr_turn_client_ensure_perm(ctx, remote_addr)))
    ABORT(r);

  if ((r=nr_transport_addr_copy(&params.remote_addr, remote_addr)))
    ABORT(r);

  params.data.data = (UCHAR*)msg;
  params.data.len = len;

  if ((r=nr_stun_build_send_indication(&params, &ind)))
    ABORT(r);

  if ((r=nr_turn_client_send_stun_request(ctx, ind, flags)))
    ABORT(r);

  _status=0;
abort:
  nr_stun_message_destroy(&ind);
  return(_status);
}

int nr_turn_client_parse_data_indication(nr_turn_client_ctx *ctx,
                                         nr_transport_addr *source_addr,
                                         UCHAR *msg, size_t len,
                                         UCHAR *newmsg, size_t *newlen,
                                         size_t newsize,
                                         nr_transport_addr *remote_addr)
{
  int r,_status;
  nr_stun_message *ind=0;
  nr_stun_message_attribute *attr;
  nr_turn_permission *perm;

  if (nr_transport_addr_cmp(&ctx->turn_server_addr, source_addr,
                            NR_TRANSPORT_ADDR_CMP_MODE_ALL)) {
    r_log(NR_LOG_TURN, LOG_WARNING,
          "TURN(%s): Indication from unexpected source addr %s (expected %s)",
          ctx->label, source_addr->as_string, ctx->turn_server_addr.as_string);
    ABORT(R_REJECTED);
  }

  if ((r=nr_stun_message_create2(&ind, msg, len)))
    ABORT(r);
  if ((r=nr_stun_decode_message(ind, 0, 0)))
    ABORT(r);

  if (ind->header.type != NR_STUN_MSG_DATA_INDICATION)
    ABORT(R_BAD_ARGS);

  if (!nr_stun_message_has_attribute(ind, NR_STUN_ATTR_XOR_PEER_ADDRESS, &attr))
    ABORT(R_BAD_ARGS);

  if ((r=nr_turn_permission_find(ctx, &attr->u.xor_mapped_address.unmasked,
                                 &perm))) {
    if (r == R_NOT_FOUND) {
      r_log(NR_LOG_TURN, LOG_WARNING,
            "TURN(%s): Indication from peer addr %s with no permission",
            ctx->label, attr->u.xor_mapped_address.unmasked.as_string);
    }
    ABORT(r);
  }

  if ((r=nr_transport_addr_copy(remote_addr,
                                &attr->u.xor_mapped_address.unmasked)))
    ABORT(r);

#if REFRESH_RESERVATION_ON_RECV
  if ((r=nr_turn_client_ensure_perm(ctx, remote_addr))) {
    ABORT(r);
  }
#endif

  if (!nr_stun_message_has_attribute(ind, NR_STUN_ATTR_DATA, &attr)) {
    ABORT(R_BAD_DATA);
  }

  assert(newsize >= attr->u.data.length);
  if (newsize < attr->u.data.length)
    ABORT(R_BAD_ARGS);

  memcpy(newmsg, attr->u.data.data, attr->u.data.length);
  *newlen = attr->u.data.length;

  _status=0;
abort:
  nr_stun_message_destroy(&ind);
  return(_status);
}



/* The permissions model is as follows:

   - We keep a list of all the permissions we have ever requested
     along with when they were last established.
   - Whenever someone sends a packet, we automatically create/
     refresh the permission.

   This means that permissions automatically time out if
   unused.

*/
int nr_turn_client_ensure_perm(nr_turn_client_ctx *ctx, const nr_transport_addr *addr)
{
  int r, _status;
  nr_turn_permission *perm = 0;
  UINT8 now;
  UINT8 turn_permission_refresh = (TURN_PERMISSION_LIFETIME_SECONDS -
                                   TURN_REFRESH_SLACK_SECONDS) * TURN_USECS_PER_S;

  if ((r=nr_turn_permission_find(ctx, addr, &perm))) {
    if (r == R_NOT_FOUND) {
      if ((r=nr_turn_permission_create(ctx, addr, &perm)))
        ABORT(r);
    }
    else {
      ABORT(r);
    }
  }

  assert(perm);

  /* Now check that the permission is up-to-date */
  now = r_gettimeint();

  if ((now - perm->last_used) > turn_permission_refresh) {
    r_log(NR_LOG_TURN, LOG_DEBUG, "TURN(%s): Permission for %s requires refresh",
          ctx->label, perm->addr.as_string);

    if ((r=nr_turn_stun_ctx_start(perm->stun)))
      ABORT(r);

    perm->last_used = now;  /* Update the time now so we don't retry on
                               next packet */
  }

  _status=0;
abort:
  return(_status);
}

static int nr_turn_permission_create(nr_turn_client_ctx *ctx, const nr_transport_addr *addr,
                                     nr_turn_permission **permp)
{
  int r, _status;
  nr_turn_permission *perm = 0;

  assert(ctx->state == NR_TURN_CLIENT_STATE_ALLOCATED);

  r_log(NR_LOG_TURN, LOG_INFO, "TURN(%s): Creating permission for %s",
        ctx->label, addr->as_string);

  if (!(perm = RCALLOC(sizeof(nr_turn_permission))))
    ABORT(R_NO_MEMORY);

  if ((r=nr_transport_addr_copy(&perm->addr, addr)))
    ABORT(r);

  perm->last_used = 0;

  if ((r=nr_turn_stun_ctx_create(ctx, NR_TURN_CLIENT_MODE_PERMISSION_REQUEST,
                                 nr_turn_client_permissions_cb,
                                 nr_turn_client_permission_error_cb,
                                 &perm->stun)))
    ABORT(r);

  /* We want to authenticate on the first packet */
  if ((r=nr_turn_stun_set_auth_params(perm->stun, ctx->realm, ctx->nonce)))
    ABORT(r);

  if ((r=nr_transport_addr_copy(
          &perm->stun->stun->params.permission_request.remote_addr, addr)))
    ABORT(r);
  STAILQ_INSERT_TAIL(&ctx->permissions, perm, entry);

  *permp = perm;

  _status=0;
abort:
  if (_status) {
    nr_turn_permission_destroy(&perm);
  }
  return(_status);
}


static int nr_turn_permission_find(nr_turn_client_ctx *ctx, const nr_transport_addr *addr,
                                   nr_turn_permission **permp)
{
  nr_turn_permission *perm;
  int _status;

  perm = STAILQ_FIRST(&ctx->permissions);
  while (perm) {
    if (!nr_transport_addr_cmp(&perm->addr, addr,
                               NR_TRANSPORT_ADDR_CMP_MODE_ADDR))
      break;

    perm = STAILQ_NEXT(perm, entry);
  }

  if (!perm) {
    ABORT(R_NOT_FOUND);
  }
  if (perm->stun->last_error_code == 403) {
    ABORT(R_NOT_PERMITTED);
  }
  *permp = perm;

  _status=0;
abort:
  return(_status);
}

static void nr_turn_client_permissions_cb(NR_SOCKET s, int how, void *arg)
{
  nr_turn_stun_ctx *ctx = (nr_turn_stun_ctx *)arg;
  r_log(NR_LOG_TURN, LOG_DEBUG, "TURN(%s): Successfully refreshed permission",
        ctx->tctx->label);
}

/* Note that we don't destroy the nr_turn_stun_ctx. That is owned by the
   nr_turn_client_ctx. */
static int nr_turn_permission_destroy(nr_turn_permission **permp)
{
  nr_turn_permission *perm;

  if (!permp || !*permp)
    return(0);

  perm = *permp;
  *permp = 0;

  RFREE(perm);

  return(0);
}

#endif /* USE_TURN */
