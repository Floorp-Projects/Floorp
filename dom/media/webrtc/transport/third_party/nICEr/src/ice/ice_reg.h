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



#ifndef _ice_reg_h
#define _ice_reg_h
#ifdef __cplusplus
using namespace std;
extern "C" {
#endif /* __cplusplus */

#define NR_ICE_REG_PREF_TYPE_HOST           "ice.pref.type.host"
#define NR_ICE_REG_PREF_TYPE_RELAYED        "ice.pref.type.relayed"
#define NR_ICE_REG_PREF_TYPE_SRV_RFLX       "ice.pref.type.srv_rflx"
#define NR_ICE_REG_PREF_TYPE_PEER_RFLX      "ice.pref.type.peer_rflx"
#define NR_ICE_REG_PREF_TYPE_HOST_TCP       "ice.pref.type.host_tcp"
#define NR_ICE_REG_PREF_TYPE_RELAYED_TCP    "ice.pref.type.relayed_tcp"
#define NR_ICE_REG_PREF_TYPE_SRV_RFLX_TCP   "ice.pref.type.srv_rflx_tcp"
#define NR_ICE_REG_PREF_TYPE_PEER_RFLX_TCP  "ice.pref.type.peer_rflx_tcp"

#define NR_ICE_REG_PREF_INTERFACE_PRFX      "ice.pref.interface"
#define NR_ICE_REG_SUPPRESS_INTERFACE_PRFX  "ice.suppress.interface"

#define NR_ICE_REG_STUN_SRV_PRFX            "ice.stun.server"
#define NR_ICE_REG_STUN_SRV_ADDR            "addr"
#define NR_ICE_REG_STUN_SRV_PORT            "port"

#define NR_ICE_REG_TURN_SRV_PRFX            "ice.turn.server"
#define NR_ICE_REG_TURN_SRV_ADDR            "addr"
#define NR_ICE_REG_TURN_SRV_PORT            "port"
#define NR_ICE_REG_TURN_SRV_BANDWIDTH       "bandwidth"
#define NR_ICE_REG_TURN_SRV_LIFETIME        "lifetime"
#define NR_ICE_REG_TURN_SRV_USERNAME        "username"
#define NR_ICE_REG_TURN_SRV_PASSWORD        "password"

#define NR_ICE_REG_ICE_TCP_DISABLE          "ice.tcp.disable"
#define NR_ICE_REG_ICE_TCP_SO_SOCK_COUNT    "ice.tcp.so_sock_count"
#define NR_ICE_REG_ICE_TCP_LISTEN_BACKLOG   "ice.tcp.listen_backlog"

#define NR_ICE_REG_KEEPALIVE_TIMER          "ice.keepalive_timer"

#define NR_ICE_REG_TRICKLE_GRACE_PERIOD     "ice.trickle_grace_period"
#define NR_ICE_REG_PREF_FORCE_INTERFACE_NAME "ice.forced_interface_name"

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif

