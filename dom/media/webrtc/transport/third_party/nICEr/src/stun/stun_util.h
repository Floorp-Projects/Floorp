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



#ifndef _stun_util_h
#define _stun_util_h

#include "stun.h"
#include "local_addr.h"

extern int NR_LOG_STUN;

int nr_stun_startup(void);

int nr_stun_xor_mapped_address(UINT4 magicCookie, UINT12 transactionId, nr_transport_addr *from, nr_transport_addr *to);

// removes duplicates, and based on prefs, loopback and link_local addresses
int nr_stun_filter_local_addresses(nr_local_addr addrs[], int *count);

int nr_stun_find_local_addresses(nr_local_addr addrs[], int maxaddrs, int *count);

int nr_stun_different_transaction(UCHAR *msg, size_t len, nr_stun_message *req);

char* nr_stun_msg_type(int type);

int nr_random_alphanum(char *alphanum, int size);

// accumulate a count without worrying about rollover
void nr_accumulate_count(UINT2* orig_count, UINT2 add_count);

#endif

