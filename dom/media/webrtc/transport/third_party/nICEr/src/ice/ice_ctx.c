/*
Copyright (c) 2007, Adobe Systems, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of Adobe Systems, Network Resonance nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

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

#include <csi_platform.h>
#include <assert.h>
#include <sys/types.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <sys/queue.h>
#include <string.h>
#include <nr_api.h>
#include <registry.h>
#include "stun.h"
#include "ice_ctx.h"
#include "ice_reg.h"
#include "nr_crypto.h"
#include "async_timer.h"
#include "util.h"
#include "nr_socket_local.h"

#define ICE_UFRAG_LEN 8
#define ICE_PWD_LEN 32

int LOG_ICE = 0;

static int nr_ice_random_string(char *str, int len);
static int nr_ice_fetch_stun_servers(int ct, nr_ice_stun_server **out);
#ifdef USE_TURN
static int nr_ice_fetch_turn_servers(int ct, nr_ice_turn_server **out);
#endif /* USE_TURN */
static int nr_ice_ctx_pair_new_trickle_candidates(nr_ice_ctx *ctx, nr_ice_candidate *cand);
static int no_op(void **obj) {
  return 0;
}

static nr_socket_factory_vtbl default_socket_factory_vtbl = {
  nr_socket_local_create,
  no_op
};

int nr_ice_fetch_stun_servers(int ct, nr_ice_stun_server **out)
  {
    int r,_status;
    nr_ice_stun_server *servers = 0;
    int i;
    NR_registry child;
    char *addr=0;
    UINT2 port;
    in_addr_t addr_int;

    if(!(servers=RCALLOC(sizeof(nr_ice_stun_server)*ct)))
      ABORT(R_NO_MEMORY);

    for(i=0;i<ct;i++){
      if(r=NR_reg_get_child_registry(NR_ICE_REG_STUN_SRV_PRFX,i,child))
        ABORT(r);
      /* Assume we have a v4 addr for now */
      if(r=NR_reg_alloc2_string(child,"addr",&addr))
        ABORT(r);
      addr_int=inet_addr(addr);
      if(addr_int==INADDR_NONE){
        r_log(LOG_ICE,LOG_ERR,"Invalid address %s;",addr);
        ABORT(R_BAD_ARGS);
      }
      if(r=NR_reg_get2_uint2(child,"port",&port)) {
        if (r != R_NOT_FOUND)
          ABORT(r);
        port = 3478;
      }
      if (r = nr_ip4_port_to_transport_addr(ntohl(addr_int), port, IPPROTO_UDP,
                                            &servers[i].addr))
        ABORT(r);
      RFREE(addr);
      addr=0;
    }

    *out = servers;

    _status=0;
  abort:
    RFREE(addr);
    if (_status) RFREE(servers);
    return(_status);
  }

int nr_ice_ctx_set_stun_servers(nr_ice_ctx *ctx,nr_ice_stun_server *servers,int ct)
  {
    int _status;

    if(ctx->stun_servers){
      RFREE(ctx->stun_servers);
      ctx->stun_server_ct=0;
    }

    if (ct) {
      if(!(ctx->stun_servers=RCALLOC(sizeof(nr_ice_stun_server)*ct)))
        ABORT(R_NO_MEMORY);

      memcpy(ctx->stun_servers,servers,sizeof(nr_ice_stun_server)*ct);
      ctx->stun_server_ct = ct;
    }

    _status=0;
 abort:
    return(_status);
  }

int nr_ice_ctx_set_turn_servers(nr_ice_ctx *ctx,nr_ice_turn_server *servers,int ct)
  {
    int _status;

    if(ctx->turn_servers){
      RFREE(ctx->turn_servers);
      ctx->turn_server_ct=0;
    }

    if(ct) {
      if(!(ctx->turn_servers=RCALLOC(sizeof(nr_ice_turn_server)*ct)))
        ABORT(R_NO_MEMORY);

      memcpy(ctx->turn_servers,servers,sizeof(nr_ice_turn_server)*ct);
      ctx->turn_server_ct = ct;
    }

    _status=0;
 abort:
    return(_status);
  }

int nr_ice_ctx_copy_turn_servers(nr_ice_ctx *ctx, nr_ice_turn_server *servers, int ct)
  {
    int _status, i, r;

    if (r = nr_ice_ctx_set_turn_servers(ctx, servers, ct)) {
      ABORT(r);
    }

    // make copies of the username and password so they aren't freed twice
    for (i = 0; i < ct; ++i) {
      if (!(ctx->turn_servers[i].username = r_strdup(servers[i].username))) {
        ABORT(R_NO_MEMORY);
      }
      if (r = r_data_create(&ctx->turn_servers[i].password,
                            servers[i].password->data,
                            servers[i].password->len)) {
        ABORT(r);
      }
    }

    _status=0;
   abort:
    return(_status);
  }

static int nr_ice_ctx_set_local_addrs(nr_ice_ctx *ctx,nr_local_addr *addrs,int ct)
  {
    int _status,i,r;

    if(ctx->local_addrs) {
      RFREE(ctx->local_addrs);
      ctx->local_addr_ct=0;
      ctx->local_addrs=0;
    }

    if (ct) {
      if(!(ctx->local_addrs=RCALLOC(sizeof(nr_local_addr)*ct)))
        ABORT(R_NO_MEMORY);

      for (i=0;i<ct;++i) {
        if (r=nr_local_addr_copy(ctx->local_addrs+i,addrs+i)) {
          ABORT(r);
        }
      }
      ctx->local_addr_ct = ct;
    }

    _status=0;
   abort:
    return(_status);
  }

int nr_ice_ctx_set_resolver(nr_ice_ctx *ctx, nr_resolver *resolver)
  {
    int _status;

    if (ctx->resolver) {
      ABORT(R_ALREADY);
    }

    ctx->resolver = resolver;

    _status=0;
   abort:
    return(_status);
  }

int nr_ice_ctx_set_interface_prioritizer(nr_ice_ctx *ctx, nr_interface_prioritizer *ip)
  {
    int _status;

    if (ctx->interface_prioritizer) {
      ABORT(R_ALREADY);
    }

    ctx->interface_prioritizer = ip;

    _status=0;
   abort:
    return(_status);
  }

void nr_ice_ctx_set_socket_factory(nr_ice_ctx *ctx, nr_socket_factory *factory)
  {
    nr_socket_factory_destroy(&ctx->socket_factory);
    ctx->socket_factory = factory;
  }

#ifdef USE_TURN
int nr_ice_fetch_turn_servers(int ct, nr_ice_turn_server **out)
  {
    int r,_status;
    nr_ice_turn_server *servers = 0;
    int i;
    NR_registry child;
    char *addr=0;
    UINT2 port;
    in_addr_t addr_int;
    Data data={0};

    if(!(servers=RCALLOC(sizeof(nr_ice_turn_server)*ct)))
      ABORT(R_NO_MEMORY);

    for(i=0;i<ct;i++){
      if(r=NR_reg_get_child_registry(NR_ICE_REG_TURN_SRV_PRFX,i,child))
        ABORT(r);
      /* Assume we have a v4 addr for now */
      if(r=NR_reg_alloc2_string(child,"addr",&addr))
        ABORT(r);
      addr_int=inet_addr(addr);
      if(addr_int==INADDR_NONE){
        r_log(LOG_ICE,LOG_ERR,"Invalid address %s",addr);
        ABORT(R_BAD_ARGS);
      }
      if(r=NR_reg_get2_uint2(child,"port",&port)) {
        if (r != R_NOT_FOUND)
          ABORT(r);
        port = 3478;
      }
      if (r = nr_ip4_port_to_transport_addr(ntohl(addr_int), port, IPPROTO_UDP,
                                            &servers[i].turn_server.addr))
        ABORT(r);


      if(r=NR_reg_alloc2_string(child,NR_ICE_REG_TURN_SRV_USERNAME,&servers[i].username)){
        if(r!=R_NOT_FOUND)
          ABORT(r);
      }

      if(r=NR_reg_alloc2_data(child,NR_ICE_REG_TURN_SRV_PASSWORD,&data)){
        if(r!=R_NOT_FOUND)
          ABORT(r);
      }
      else {
        servers[i].password=RCALLOC(sizeof(*servers[i].password));
        if(!servers[i].password)
          ABORT(R_NO_MEMORY);
        servers[i].password->data = data.data;
        servers[i].password->len = data.len;
        data.data=0;
      }

      RFREE(addr);
      addr=0;
    }

    *out = servers;

    _status=0;
  abort:
    RFREE(data.data);
    RFREE(addr);
    if (_status) RFREE(servers);
    return(_status);
  }
#endif /* USE_TURN */

#define MAXADDRS 100 /* Ridiculously high */
int nr_ice_ctx_create(char *label, UINT4 flags, nr_ice_ctx **ctxp)
  {
    nr_ice_ctx *ctx=0;
    int r,_status;

    if(r=r_log_register("ice", &LOG_ICE))
      ABORT(r);

    if(!(ctx=RCALLOC(sizeof(nr_ice_ctx))))
      ABORT(R_NO_MEMORY);

    ctx->flags=flags;

    if(!(ctx->label=r_strdup(label)))
      ABORT(R_NO_MEMORY);

    /* Get the STUN servers */
    if(r=NR_reg_get_child_count(NR_ICE_REG_STUN_SRV_PRFX,
      (unsigned int *)&ctx->stun_server_ct)||ctx->stun_server_ct==0) {
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): No STUN servers specified in nICEr registry", ctx->label);
      ctx->stun_server_ct=0;
    }

    /* 31 is the max for our priority algorithm */
    if(ctx->stun_server_ct>31){
      r_log(LOG_ICE,LOG_WARNING,"ICE(%s): Too many STUN servers specified: max=31", ctx->label);
      ctx->stun_server_ct=31;
    }

    if(ctx->stun_server_ct>0){
      if(r=nr_ice_fetch_stun_servers(ctx->stun_server_ct,&ctx->stun_servers)){
        r_log(LOG_ICE,LOG_ERR,"ICE(%s): Couldn't load STUN servers from registry", ctx->label);
        ctx->stun_server_ct=0;
        ABORT(r);
      }
    }

#ifdef USE_TURN
    /* Get the TURN servers */
    if(r=NR_reg_get_child_count(NR_ICE_REG_TURN_SRV_PRFX,
      (unsigned int *)&ctx->turn_server_ct)||ctx->turn_server_ct==0) {
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): No TURN servers specified in nICEr registry", ctx->label);
      ctx->turn_server_ct=0;
    }
#else
    ctx->turn_server_ct=0;
#endif /* USE_TURN */

    ctx->local_addrs=0;
    ctx->local_addr_ct=0;

    /* 31 is the max for our priority algorithm */
    if((ctx->stun_server_ct+ctx->turn_server_ct)>31){
      r_log(LOG_ICE,LOG_WARNING,"ICE(%s): Too many STUN/TURN servers specified: max=31", ctx->label);
      ctx->turn_server_ct=31-ctx->stun_server_ct;
    }

#ifdef USE_TURN
    if(ctx->turn_server_ct>0){
      if(r=nr_ice_fetch_turn_servers(ctx->turn_server_ct,&ctx->turn_servers)){
        ctx->turn_server_ct=0;
        r_log(LOG_ICE,LOG_ERR,"ICE(%s): Couldn't load TURN servers from registry", ctx->label);
        ABORT(r);
      }
    }
#endif /* USE_TURN */


    ctx->Ta = 20;

    ctx->test_timer_divider = 0;

    if (r=nr_socket_factory_create_int(NULL, &default_socket_factory_vtbl, &ctx->socket_factory))
      ABORT(r);

    if ((r=NR_reg_get_string((char *)NR_ICE_REG_PREF_FORCE_INTERFACE_NAME, ctx->force_net_interface, sizeof(ctx->force_net_interface)))) {
      if (r == R_NOT_FOUND) {
        ctx->force_net_interface[0] = 0;
      } else {
        ABORT(r);
      }
    }

    ctx->target_for_default_local_address_lookup=0;

    STAILQ_INIT(&ctx->streams);
    STAILQ_INIT(&ctx->sockets);
    STAILQ_INIT(&ctx->foundations);
    STAILQ_INIT(&ctx->peers);
    STAILQ_INIT(&ctx->ids);

    *ctxp=ctx;

    _status=0;
  abort:
    if (_status && ctx) nr_ice_ctx_destroy(&ctx);

    return(_status);
  }

  void nr_ice_ctx_add_flags(nr_ice_ctx* ctx, UINT4 flags) {
    ctx->flags |= flags;
  }

  void nr_ice_ctx_remove_flags(nr_ice_ctx* ctx, UINT4 flags) {
    ctx->flags &= ~flags;
  }

  void nr_ice_ctx_destroy(nr_ice_ctx** ctxp) {
    if (!ctxp || !*ctxp) return;

    nr_ice_ctx* ctx = *ctxp;
    nr_ice_foundation *f1,*f2;
    nr_ice_media_stream *s1,*s2;
    int i;
    nr_ice_stun_id *id1,*id2;

    ctx->done_cb = 0;
    ctx->trickle_cb = 0;

    STAILQ_FOREACH_SAFE(s1, &ctx->streams, entry, s2){
      STAILQ_REMOVE(&ctx->streams,s1,nr_ice_media_stream_,entry);
      nr_ice_media_stream_destroy(&s1);
    }

    RFREE(ctx->label);

    RFREE(ctx->stun_servers);

    RFREE(ctx->local_addrs);

    RFREE(ctx->target_for_default_local_address_lookup);

    for (i = 0; i < ctx->turn_server_ct; i++) {
        RFREE(ctx->turn_servers[i].username);
        r_data_destroy(&ctx->turn_servers[i].password);
    }
    RFREE(ctx->turn_servers);

    f1=STAILQ_FIRST(&ctx->foundations);
    while(f1){
      f2=STAILQ_NEXT(f1,entry);
      RFREE(f1);
      f1=f2;
    }

    STAILQ_FOREACH_SAFE(id1, &ctx->ids, entry, id2){
      STAILQ_REMOVE(&ctx->ids,id1,nr_ice_stun_id_,entry);
      RFREE(id1);
    }

    nr_resolver_destroy(&ctx->resolver);
    nr_interface_prioritizer_destroy(&ctx->interface_prioritizer);
    nr_socket_factory_destroy(&ctx->socket_factory);

    RFREE(ctx);

    *ctxp=0;
  }

void nr_ice_gather_finished_cb(NR_SOCKET s, int h, void *cb_arg)
  {
    int r;
    nr_ice_candidate *cand=cb_arg;
    nr_ice_ctx *ctx;
    nr_ice_media_stream *stream;
    int component_id;

    assert(cb_arg);
    if (!cb_arg)
      return;
    ctx = cand->ctx;
    stream = cand->stream;
    component_id = cand->component_id;

    ctx->uninitialized_candidates--;
    if (cand->state == NR_ICE_CAND_STATE_FAILED) {
      r_log(LOG_ICE, LOG_WARNING,
            "ICE(%s)/CAND(%s): failed to initialize, %d remaining", ctx->label,
            cand->label, ctx->uninitialized_candidates);
    } else {
      r_log(LOG_ICE, LOG_DEBUG, "ICE(%s)/CAND(%s): initialized, %d remaining",
            ctx->label, cand->label, ctx->uninitialized_candidates);
    }

    /* Avoid the need for yet another initialization function */
    if (cand->state == NR_ICE_CAND_STATE_INITIALIZING && cand->type == HOST)
      cand->state = NR_ICE_CAND_STATE_INITIALIZED;

    if (cand->state == NR_ICE_CAND_STATE_INITIALIZED) {
      int was_pruned = 0;

      if (r=nr_ice_component_maybe_prune_candidate(ctx, cand->component,
                                                   cand, &was_pruned)) {
          r_log(LOG_ICE, LOG_NOTICE, "ICE(%s): Problem pruning candidates",ctx->label);
      }

      if (was_pruned) {
        cand = NULL;
      }

      /* If we are initialized, the candidate wasn't pruned,
         and we have a trickle ICE callback fire the callback */
      if (ctx->trickle_cb && cand &&
          !nr_ice_ctx_hide_candidate(ctx, cand)) {
        ctx->trickle_cb(ctx->trickle_cb_arg, ctx, cand->stream, cand->component_id, cand);

        if (nr_ice_ctx_pair_new_trickle_candidates(ctx, cand)) {
          r_log(LOG_ICE,LOG_ERR, "ICE(%s): All could not pair new trickle candidate",ctx->label);
          /* But continue */
        }
      }
    }

    if (nr_ice_media_stream_is_done_gathering(stream) &&
        ctx->trickle_cb) {
      ctx->trickle_cb(ctx->trickle_cb_arg, ctx, stream, component_id, NULL);
    }

    if(ctx->uninitialized_candidates==0){
      r_log(LOG_ICE, LOG_INFO, "ICE(%s): All candidates initialized",
            ctx->label);
      if (ctx->done_cb) {
        ctx->done_cb(0,0,ctx->cb_arg);
      }
      else {
        r_log(LOG_ICE, LOG_INFO,
              "ICE(%s): No done_cb. We were probably destroyed.", ctx->label);
      }
    }
    else {
      r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): Waiting for %d candidates to be initialized",ctx->label, ctx->uninitialized_candidates);
    }
  }

static int nr_ice_ctx_pair_new_trickle_candidates(nr_ice_ctx *ctx, nr_ice_candidate *cand)
  {
    int r,_status;
    nr_ice_peer_ctx *pctx;

    pctx=STAILQ_FIRST(&ctx->peers);
    while(pctx){
      if (pctx->state == NR_ICE_PEER_STATE_PAIRED) {
        r = nr_ice_peer_ctx_pair_new_trickle_candidate(ctx, pctx, cand);
        if (r)
          ABORT(r);
      }

      pctx=STAILQ_NEXT(pctx,entry);
    }

    _status=0;
 abort:
    return(_status);
  }

/* Get the default address by creating a UDP socket, binding it to a wildcard
   address, and connecting it to the remote IP. Because this is UDP, no packets
   are sent. This lets us query the local address assigned to the socket by the
   kernel.

   If the context's remote address is NULL, then the application wasn't loaded
   over the network, and we can fall back on connecting to a known public
   address (namely Google's):

   IPv4: 8.8.8.8
   IPv6: 2001:4860:4860::8888
*/
static int nr_ice_get_default_address(nr_ice_ctx *ctx, int ip_version, nr_transport_addr* addrp)
  {
    int r,_status;
    nr_transport_addr addr, known_remote_addr;
    nr_transport_addr *remote_addr=ctx->target_for_default_local_address_lookup;
    nr_socket *sock=0;

    switch(ip_version) {
      case NR_IPV4:
        if ((r=nr_str_port_to_transport_addr("0.0.0.0", 0, IPPROTO_UDP, &addr)))
          ABORT(r);
        if (!remote_addr || nr_transport_addr_is_loopback(remote_addr)) {
          if ((r=nr_str_port_to_transport_addr("8.8.8.8", 53, IPPROTO_UDP, &known_remote_addr)))
            ABORT(r);
          remote_addr=&known_remote_addr;
        }
        break;
      case NR_IPV6:
        if ((r=nr_str_port_to_transport_addr("::0", 0, IPPROTO_UDP, &addr)))
          ABORT(r);
        if (!remote_addr || nr_transport_addr_is_loopback(remote_addr)) {
          if ((r=nr_str_port_to_transport_addr("2001:4860:4860::8888", 53, IPPROTO_UDP, &known_remote_addr)))
            ABORT(r);
          remote_addr=&known_remote_addr;
        }
        break;
      default:
        assert(0);
        ABORT(R_INTERNAL);
    }

    if ((r=nr_socket_factory_create_socket(ctx->socket_factory, &addr, &sock)))
      ABORT(r);
    if ((r=nr_socket_connect(sock, remote_addr)))
      ABORT(r);
    if ((r=nr_socket_getaddr(sock, addrp)))
      ABORT(r);

    r_log(LOG_GENERIC, LOG_DEBUG, "Default address: %s", addrp->as_string);

    _status=0;
  abort:
    nr_socket_destroy(&sock);
    return(_status);
  }

static int nr_ice_get_default_local_address(nr_ice_ctx *ctx, int ip_version, nr_local_addr* addrs, int addr_ct, nr_local_addr *addrp)
  {
    int r,_status;
    nr_transport_addr default_addr;
    int i;

    if ((r=nr_ice_get_default_address(ctx, ip_version, &default_addr)))
        ABORT(r);

    for (i=0; i < addr_ct; ++i) {
      // if default addr is found in local addrs, copy the more fully
      // complete local addr to the output arg.  Don't need to worry
      // about comparing ports here.
      if (!nr_transport_addr_cmp(&default_addr, &addrs[i].addr,
                                 NR_TRANSPORT_ADDR_CMP_MODE_ADDR)) {
        if ((r=nr_local_addr_copy(addrp, &addrs[i])))
          ABORT(r);
        break;
      }
    }

    // if default addr is not in local addrs, just copy the transport addr
    // to output arg.
    if (i == addr_ct) {
      if ((r=nr_transport_addr_copy(&addrp->addr, &default_addr)))
        ABORT(r);
      (void)strlcpy(addrp->addr.ifname, "default route", sizeof(addrp->addr.ifname));
    }

    _status=0;
  abort:
    return(_status);
  }

/* if handed a IPv4 default_local_addr, looks for IPv6 address on same interface
   if handed a IPv6 default_local_addr, looks for IPv4 address on same interface
*/
static int nr_ice_get_assoc_interface_address(nr_local_addr* default_local_addr,
                                              nr_local_addr* local_addrs, int addr_ct,
                                              nr_local_addr* assoc_addrp)
  {
    int r, _status;
    int i, ip_version;

    if (!default_local_addr || !local_addrs || !addr_ct) {
      ABORT(R_BAD_ARGS);
    }

    /* set _status to R_EOD in case we don't find an associated address */
    _status = R_EOD;

    /* look for IPv6 if we have IPv4, look for IPv4 if we have IPv6 */
    ip_version = (NR_IPV4 == default_local_addr->addr.ip_version?NR_IPV6:NR_IPV4);

    for (i=0; i<addr_ct; ++i) {
      /* if we find the ip_version we're looking for on the matching interface,
         copy it to assoc_addrp.
      */
      if (local_addrs[i].addr.ip_version == ip_version &&
          !strcmp(local_addrs[i].addr.ifname, default_local_addr->addr.ifname)) {
        if (r=nr_local_addr_copy(assoc_addrp, &local_addrs[i])) {
          ABORT(r);
        }
        _status = 0;
        break;
      }
    }

  abort:
    return(_status);
  }

int nr_ice_set_local_addresses(nr_ice_ctx *ctx,
                               nr_local_addr* stun_addrs, int stun_addr_ct)
  {
    int r,_status;
    nr_local_addr local_addrs[MAXADDRS];
    nr_local_addr *addrs = 0;
    int i,addr_ct;
    nr_local_addr default_addrs[2];
    int default_addr_ct = 0;

    if (!stun_addrs || !stun_addr_ct) {
      r_log(LOG_ICE,LOG_ERR,"ICE(%s): no stun addrs provided",ctx->label);
      ABORT(R_BAD_ARGS);
    }

    addr_ct = MIN(stun_addr_ct, MAXADDRS);
    r_log(LOG_ICE, LOG_DEBUG, "ICE(%s): copy %d pre-fetched stun addrs", ctx->label, addr_ct);
    for (i=0; i<addr_ct; ++i) {
      if (r=nr_local_addr_copy(&local_addrs[i], &stun_addrs[i])) {
        ABORT(r);
      }
    }

    // removes duplicates and, based on prefs, loopback and link_local addrs
    if (r=nr_stun_filter_local_addresses(local_addrs, &addr_ct)) {
      ABORT(r);
    }

    if (ctx->force_net_interface[0] && addr_ct) {
      /* Limit us to only addresses on a single interface */
      int force_addr_ct = 0;
      for(i=0;i<addr_ct;i++){
        if (!strcmp(local_addrs[i].addr.ifname, ctx->force_net_interface)) {
          // copy it down in the array, if needed
          if (i != force_addr_ct) {
            if (r=nr_local_addr_copy(&local_addrs[force_addr_ct], &local_addrs[i])) {
              ABORT(r);
            }
          }
          force_addr_ct++;
        }
      }
      addr_ct = force_addr_ct;
    }

    r_log(LOG_ICE, LOG_DEBUG,
          "ICE(%s): use only default local addresses: %s\n",
          ctx->label,
          (char*)(ctx->flags & NR_ICE_CTX_FLAGS_ONLY_DEFAULT_ADDRS?"yes":"no"));
    if ((!addr_ct) || (ctx->flags & NR_ICE_CTX_FLAGS_ONLY_DEFAULT_ADDRS)) {
      if (ctx->target_for_default_local_address_lookup) {
        /* Get just the default IPv4 or IPv6 addr */
        if(!nr_ice_get_default_local_address(
               ctx, ctx->target_for_default_local_address_lookup->ip_version,
               local_addrs, addr_ct, &default_addrs[default_addr_ct])) {
          nr_local_addr *new_addr = &default_addrs[default_addr_ct];

          ++default_addr_ct;

          /* If we have a default target address, check for an associated
             address on the same interface.  For example, if the default
             target address is IPv6, this will find an associated IPv4
             address on the same interface.
             This makes ICE w/ dual stacks work better - Bug 1609124.
          */
          if(!nr_ice_get_assoc_interface_address(
              new_addr, local_addrs, addr_ct,
              &default_addrs[default_addr_ct])) {
            ++default_addr_ct;
          }
        }
      } else {
        /* Get just the default IPv4 and IPv6 addrs */
        if(!nr_ice_get_default_local_address(ctx, NR_IPV4, local_addrs, addr_ct,
                                             &default_addrs[default_addr_ct])) {
          ++default_addr_ct;
        }
        if(!nr_ice_get_default_local_address(ctx, NR_IPV6, local_addrs, addr_ct,
                                             &default_addrs[default_addr_ct])) {
          ++default_addr_ct;
        }
      }
      if (!default_addr_ct) {
        r_log(LOG_ICE,LOG_ERR,"ICE(%s): failed to find default addresses",ctx->label);
        ABORT(R_FAILED);
      }
      addrs = default_addrs;
      addr_ct = default_addr_ct;
    }
    else {
      addrs = local_addrs;
    }

    /* Sort interfaces by preference */
    if(ctx->interface_prioritizer) {
      for(i=0;i<addr_ct;i++){
        if((r=nr_interface_prioritizer_add_interface(ctx->interface_prioritizer,addrs+i)) && (r!=R_ALREADY)) {
          r_log(LOG_ICE,LOG_ERR,"ICE(%s): unable to add interface ",ctx->label);
          ABORT(r);
        }
      }
      if(r=nr_interface_prioritizer_sort_preference(ctx->interface_prioritizer)) {
        r_log(LOG_ICE,LOG_ERR,"ICE(%s): unable to sort interface by preference",ctx->label);
        ABORT(r);
      }
    }

    if (r=nr_ice_ctx_set_local_addrs(ctx,addrs,addr_ct)) {
      ABORT(r);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_set_target_for_default_local_address_lookup(nr_ice_ctx *ctx, const char *target_ip, UINT2 target_port)
  {
    int r,_status;

    if (ctx->target_for_default_local_address_lookup) {
      RFREE(ctx->target_for_default_local_address_lookup);
      ctx->target_for_default_local_address_lookup=0;
    }

    if (!(ctx->target_for_default_local_address_lookup=RCALLOC(sizeof(nr_transport_addr))))
      ABORT(R_NO_MEMORY);

    if ((r=nr_str_port_to_transport_addr(target_ip, target_port, IPPROTO_UDP, ctx->target_for_default_local_address_lookup))) {
      RFREE(ctx->target_for_default_local_address_lookup);
      ctx->target_for_default_local_address_lookup=0;
      ABORT(r);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_gather(nr_ice_ctx *ctx, NR_async_cb done_cb, void *cb_arg)
  {
    int r,_status;
    nr_ice_media_stream *stream;
    nr_local_addr stun_addrs[MAXADDRS];
    int stun_addr_ct;

    if (!ctx->local_addrs) {
      if((r=nr_stun_find_local_addresses(stun_addrs,MAXADDRS,&stun_addr_ct))) {
        ABORT(r);
      }
      if((r=nr_ice_set_local_addresses(ctx,stun_addrs,stun_addr_ct))) {
        ABORT(r);
      }
    }

    if(STAILQ_EMPTY(&ctx->streams)) {
      r_log(LOG_ICE,LOG_ERR,"ICE(%s): Missing streams to initialize",ctx->label);
      ABORT(R_BAD_ARGS);
    }

    r_log(LOG_ICE,LOG_DEBUG,"ICE(%s): Initializing candidates",ctx->label);
    ctx->done_cb=done_cb;
    ctx->cb_arg=cb_arg;

    /* Initialize all the media stream/component pairs */
    stream=STAILQ_FIRST(&ctx->streams);
    while(stream){
      if(!stream->obsolete) {
        if(r=nr_ice_media_stream_initialize(ctx,stream)) {
          ABORT(r);
        }
      }

      stream=STAILQ_NEXT(stream,entry);
    }

    if(ctx->uninitialized_candidates)
      ABORT(R_WOULDBLOCK);

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_add_media_stream(nr_ice_ctx *ctx,const char *label,const char *ufrag,const char *pwd,int components, nr_ice_media_stream **streamp)
  {
    int r,_status;

    if(r=nr_ice_media_stream_create(ctx,label,ufrag,pwd,components,streamp))
      ABORT(r);

    STAILQ_INSERT_TAIL(&ctx->streams,*streamp,entry);

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_remove_media_stream(nr_ice_ctx *ctx,nr_ice_media_stream **streamp)
  {
    int r,_status;
    nr_ice_peer_ctx *pctx;
    nr_ice_media_stream *peer_stream;

    pctx=STAILQ_FIRST(&ctx->peers);
    while(pctx){
      if(!nr_ice_peer_ctx_find_pstream(pctx, *streamp, &peer_stream)) {
        if(r=nr_ice_peer_ctx_remove_pstream(pctx, &peer_stream)) {
          ABORT(r);
        }
      }

      pctx=STAILQ_NEXT(pctx,entry);
    }

    STAILQ_REMOVE(&ctx->streams,*streamp,nr_ice_media_stream_,entry);
    if(r=nr_ice_media_stream_destroy(streamp)) {
      ABORT(r);
    }

    _status=0;
  abort:
    return(_status);
  }

int nr_ice_get_global_attributes(nr_ice_ctx *ctx,char ***attrsp, int *attrctp)
  {
    *attrctp=0;
    *attrsp=0;
    return(0);
  }

static int nr_ice_random_string(char *str, int len)
  {
    unsigned char bytes[100];
    size_t needed;
    int r,_status;

    if(len%2) ABORT(R_BAD_ARGS);
    needed=len/2;

    if(needed>sizeof(bytes)) ABORT(R_BAD_ARGS);

    if(r=nr_crypto_random_bytes(bytes,needed))
      ABORT(r);

    if(r=nr_bin2hex(bytes,needed,(unsigned char *)str))
      ABORT(r);

    _status=0;
  abort:
    return(_status);
  }

/* This is incredibly annoying: we now have a datagram but we don't
   know which peer it's from, and we need to be able to tell the
   API user. So, offer it to each peer and if one bites, assume
   the others don't want it
*/
int nr_ice_ctx_deliver_packet(nr_ice_ctx *ctx, nr_ice_component *comp, nr_transport_addr *source_addr, UCHAR *data, int len)
  {
    nr_ice_peer_ctx *pctx;
    int r;

    pctx=STAILQ_FIRST(&ctx->peers);
    while(pctx){
      r=nr_ice_peer_ctx_deliver_packet_maybe(pctx, comp, source_addr, data, len);
      if(!r)
        break;

      pctx=STAILQ_NEXT(pctx,entry);
    }

    if(!pctx)
      r_log(LOG_ICE,LOG_WARNING,"ICE(%s): Packet received from %s which doesn't match any known peer",ctx->label,source_addr->as_string);

    return(0);
  }

int nr_ice_ctx_is_known_id(nr_ice_ctx *ctx, UCHAR id[12])
  {
    nr_ice_stun_id *xid;

    xid=STAILQ_FIRST(&ctx->ids);
    while(xid){
      if (!memcmp(xid->id, id, 12))
          return 1;

      xid=STAILQ_NEXT(xid,entry);
    }

    return 0;
  }

int nr_ice_ctx_remember_id(nr_ice_ctx *ctx, nr_stun_message *msg)
{
    int _status;
    nr_ice_stun_id *xid;

    xid = RCALLOC(sizeof(*xid));
    if (!xid)
        ABORT(R_NO_MEMORY);

    assert(sizeof(xid->id) == sizeof(msg->header.id));
#if __STDC_VERSION__ >= 201112L
    _Static_assert(sizeof(xid->id) == sizeof(msg->header.id),"Message ID Size Mismatch");
#endif
    memcpy(xid->id, &msg->header.id, sizeof(xid->id));

    STAILQ_INSERT_TAIL(&ctx->ids,xid,entry);

    _status=0;
  abort:
    return(_status);
}


/* Clean up some of the resources (mostly file descriptors) used
   by candidates we didn't choose. Note that this still leaves
   a fair amount of non-system stuff floating around. This gets
   cleaned up when you destroy the ICE ctx */
int nr_ice_ctx_finalize(nr_ice_ctx *ctx, nr_ice_peer_ctx *pctx)
  {
    nr_ice_media_stream *lstr,*rstr;

    r_log(LOG_ICE,LOG_DEBUG,"Finalizing ICE ctx %s, peer=%s",ctx->label,pctx->label);
    /*
       First find the peer stream, if any
    */
    lstr=STAILQ_FIRST(&ctx->streams);
    while(lstr){
      rstr=STAILQ_FIRST(&pctx->peer_streams);

      while(rstr){
        if(rstr->local_stream==lstr)
          break;

        rstr=STAILQ_NEXT(rstr,entry);
      }

      nr_ice_media_stream_finalize(lstr,rstr);

      lstr=STAILQ_NEXT(lstr,entry);
    }

    return(0);
  }


int nr_ice_ctx_set_trickle_cb(nr_ice_ctx *ctx, nr_ice_trickle_candidate_cb cb, void *cb_arg)
{
  ctx->trickle_cb = cb;
  ctx->trickle_cb_arg = cb_arg;

  return 0;
}

int nr_ice_ctx_hide_candidate(nr_ice_ctx *ctx, nr_ice_candidate *cand)
  {
    if (cand->state != NR_ICE_CAND_STATE_INITIALIZED) {
      return 1;
    }

    if (ctx->flags & NR_ICE_CTX_FLAGS_HIDE_HOST_CANDIDATES) {
      if (cand->type == HOST)
        return 1;
    }

    if (cand->stream->obsolete) {
      return 1;
    }

    return 0;
  }

int nr_ice_get_new_ice_ufrag(char** ufrag)
  {
    int r,_status;
    char buf[ICE_UFRAG_LEN+1];

    if(r=nr_ice_random_string(buf,ICE_UFRAG_LEN))
      ABORT(r);
    if(!(*ufrag=r_strdup(buf)))
      ABORT(r);

    _status=0;
  abort:
    if(_status) {
      RFREE(*ufrag);
      *ufrag = 0;
    }
    return(_status);
  }

int nr_ice_get_new_ice_pwd(char** pwd)
  {
    int r,_status;
    char buf[ICE_PWD_LEN+1];

    if(r=nr_ice_random_string(buf,ICE_PWD_LEN))
      ABORT(r);
    if(!(*pwd=r_strdup(buf)))
      ABORT(r);

    _status=0;
  abort:
    if(_status) {
      RFREE(*pwd);
      *pwd = 0;
    }
    return(_status);
  }
