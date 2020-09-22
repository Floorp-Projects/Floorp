/*
 *
 *    nr_reg_keys.h
 *
 *    $Source: /Users/ekr/tmp/nrappkit-dump/nrappkit/src/share/nr_reg_keys.h,v $
 *    $Revision: 1.3 $
 *    $Date: 2008/01/29 00:34:00 $
 *
 *
 *    Copyright (C) 2006, Network Resonance, Inc.
 *    All Rights Reserved
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions
 *    are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of Network Resonance, Inc. nor the name of any
 *       contributors to this software may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

#ifndef __NR_REG_KEYS_H__
#define __NR_REG_KEYS_H__

#include <stdio.h>

#define NR_REG_NAME_LENGTH_MIN                                        1
#define NR_REG_NAME_LENGTH_MAX                                        32

#define NR_REG_HOSTNAME            "hostname"
#define NR_REG_ADDRESS             "address"
#define NR_REG_NETMASKSIZE         "netmasksize"
#define NR_REG_ADDRESS_NETMASKSIZE "address/netmasksize"   /* use only in clic files */
#define NR_REG_PORT                "port"

#define NR_REG_LOGGING_SYSLOG_ENABLED                                 "logging.syslog.enabled"
#define NR_REG_LOGGING_SYSLOG_SERVERS                                 "logging.syslog.servers"
#define NR_REG_LOGGING_SYSLOG_FACILITY                                "logging.syslog.facility"
#define NR_REG_LOGGING_SYSLOG_LEVEL                                   "logging.syslog.level"

#define NR_REG_CAPTURED_DAEMONS                                       "captured.daemons"

#define NR_REG_LISTEND_ENABLED                                        "listend.enabled"

#define NR_REG_LISTEND_MAX_INPUT_BUFFER_SIZE                          "listend.max_input_buffer_size"
#define NR_REG_LISTEND_MAX_INPUT_BUFFER_SIZE_MIN                      1               // 1 byte?
#define NR_REG_LISTEND_MAX_INPUT_BUFFER_SIZE_MAX                      (10ULL*1024*1024*1024)     // 10 GB

#define NR_REG_LISTEND_INTERFACE                                      "listend.interface"
#define NR_REG_LISTEND_INTERFACE_PRIMARY                              "listend.interface.primary"
#define NR_REG_LISTEND_INTERFACE_SECONDARY                            "listend.interface.secondary"
#define NR_REG_LISTEND_LISTEN_ON_BOTH_INTERFACES                      "listend.interface.listen_on_both_interfaces"
#define NR_REG_LISTEND_ENABLE_VLAN                                    "listend.enable_vlan"

#define NR_REG_LISTEND_LISTEN_TO                                      "listend.listen_to"
#define NR_REG_LISTEND_IGNORE                                         "listend.ignore"

#define NR_REG_LISTEND_PORT_MIN                                       0
#define NR_REG_LISTEND_PORT_MAX                                       65535

#define NR_REG_REASSD_MAX_MEMORY_CONSUMPTION                          "reassd.max_memory_consumption"
#define NR_REG_REASSD_MAX_MEMORY_CONSUMPTION_MIN                      (10*1024)    // 100 KB
#define NR_REG_REASSD_MAX_MEMORY_CONSUMPTION_MAX                      (10ULL*1024*1024*1024)     // 10 GB

#define NR_REG_REASSD_DECODER_TCP_IGNORE_CHECKSUMS                    "reassd.decoder.tcp.ignore_checksums"

#define NR_REG_REASSD_DECODER_TCP_MAX_CONNECTIONS_IN_SYN_STATE        "reassd.decoder.tcp.max_connections_in_syn_state"
#define NR_REG_REASSD_DECODER_TCP_MAX_CONNECTIONS_IN_SYN_STATE_MIN    1
#define NR_REG_REASSD_DECODER_TCP_MAX_CONNECTIONS_IN_SYN_STATE_MAX    500000

#define NR_REG_REASSD_DECODER_TCP_MAX_SIMULTANEOUS_CONNECTIONS        "reassd.decoder.tcp.max_simultaneous_connections"
#define NR_REG_REASSD_DECODER_TCP_MAX_SIMULTANEOUS_CONNECTIONS_MIN    1
#define NR_REG_REASSD_DECODER_TCP_MAX_SIMULTANEOUS_CONNECTIONS_MAX    1000000  // 1 million

#define NR_REG_REASSD_DECODER_SSL_MAX_SESSION_CACHE_SIZE              "reassd.decoder.ssl.max_session_cache_size"
#define NR_REG_REASSD_DECODER_SSL_MAX_SESSION_CACHE_SIZE_MIN          0
#define NR_REG_REASSD_DECODER_SSL_MAX_SESSION_CACHE_SIZE_MAX          (1ULL*1024*1024*1024)   // 1GB

#define NR_REG_REASSD_DECODER_SSL_REVEAL_LOCAL_KEYS                   "reassd.decoder.ssl.reveal.local.keys"

#define NR_REG_REASSD_DECODER_HTTP_HANGING_RESPONSE_TIMEOUT           "reassd.decoder.http.hanging_response_timeout"
#define NR_REG_REASSD_DECODER_HTTP_HANGING_RESPONSE_TIMEOUT_MIN       1
#define NR_REG_REASSD_DECODER_HTTP_HANGING_RESPONSE_TIMEOUT_MAX       1023

#define NR_REG_REASSD_DECODER_HTTP_HANGING_TRANSMISSION_TIMEOUT       "reassd.decoder.http.hanging_transmission_timeout"
#define NR_REG_REASSD_DECODER_HTTP_HANGING_TRANSMISSION_TIMEOUT_MIN   1
#define NR_REG_REASSD_DECODER_HTTP_HANGING_TRANSMISSION_TIMEOUT_MAX   1023

#define NR_REG_REASSD_DECODER_HTTP_MAX_HTTP_MESSAGE_SIZE              "reassd.decoder.http.max_http_message_size"
#define NR_REG_REASSD_DECODER_HTTP_MAX_HTTP_MESSAGE_SIZE_MIN          0
#define NR_REG_REASSD_DECODER_HTTP_MAX_HTTP_MESSAGE_SIZE_MAX          (10ULL*1024*1024*1024)

/* PCE-only: */
#define NR_REG_LISTEND_ARCHIIVE                                       "listend.archive"
#define NR_REG_LISTEND_ARCHIVE_MAX_SIZE                               "listend.archive.max_size"
#define NR_REG_LISTEND_ARCHIVE_MAX_SIZE_MIN                           0
#define NR_REG_LISTEND_ARCHIVE_MAX_SIZE_MAX                           (10ULL*1024*1024*1024*1024)  // 10 TB

#define NR_REG_LISTEND_ARCHIVE_RECORDING_ENABLED                      "listend.archive.recording_enabled"

#define NR_REG_REASSD_DECODER_NET_DELIVER_BATCH_INTERVAL              "reassd.decoder.net_deliver.batch_interval"
#define NR_REG_REASSD_DECODER_NET_DELIVER_BATCH_INTERVAL_MIN          0
#define NR_REG_REASSD_DECODER_NET_DELIVER_BATCH_INTERVAL_MAX          1023

#define NR_REG_REASSD_DECODER_NET_DELIVER_MAX_QUEUE_DEPTH             "reassd.decoder.net_deliver.max_queue_depth"
#define NR_REG_REASSD_DECODER_NET_DELIVER_MAX_QUEUE_DEPTH_MIN         0
#define NR_REG_REASSD_DECODER_NET_DELIVER_MAX_QUEUE_DEPTH_MAX         (1ULL*1024*1024*1024)     // 1 GB

#define NR_REG_REASSD_DECODER_NET_DELIVER_MY_KEY_CERTIFICATE          "reassd.decoder.net_deliver.my_key.certificate"
#define NR_REG_REASSD_DECODER_NET_DELIVER_MY_KEY_PRIVATE_KEY          "reassd.decoder.net_deliver.my_key.private_key"
#define NR_REG_REASSD_DECODER_NET_DELIVER_PEER                        "reassd.decoder.net_deliver.peer"
#define NR_REG_REASSD_DECODER_NET_DELIVER_PEER_PORT_MIN               0
#define NR_REG_REASSD_DECODER_NET_DELIVER_PEER_PORT_MAX               65535

#define NR_REG_REASSD_DECODER_NET_DELIVER_POLLING_INTERVAL            "reassd.decoder.net_deliver.polling_interval"
#define NR_REG_REASSD_DECODER_NET_DELIVER_POLLING_INTERVAL_MIN        1
#define NR_REG_REASSD_DECODER_NET_DELIVER_POLLING_INTERVAL_MAX        1023

#define NR_REG_REASSD_DECODER_NET_DELIVER_STATELESS                   "reassd.decoder.net_deliver.stateless"
#define NR_REG_REASSD_DECODER_NET_DELIVER_WATCHDOG_TIMER              "reassd.decoder.net_deliver.watchdog_timer"
#define NR_REG_REASSD_DECODER_NET_DELIVER_WATCHDOG_TIMER_MIN          0
#define NR_REG_REASSD_DECODER_NET_DELIVER_WATCHDOG_TIMER_MAX          666

/* ASA-only: */
#define NR_REG_MIGRATE_ENABLED                                        "migrate.enabled"

#define NR_REG_MIGRATE_INACTIVITY_TIMEOUT                             "migrate.inactivity_timeout"
#define NR_REG_MIGRATE_INACTIVITY_TIMEOUT_MIN                         0
#define NR_REG_MIGRATE_INACTIVITY_TIMEOUT_MAX                         1023

#define NR_REG_MIGRATE_MIN_LOCAL_SIZE                                 "migrate.min_local_size"
#define NR_REG_MIGRATE_MIN_OVERLAP_SIZE                               "migrate.min_overlap_size"
#define NR_REG_MIGRATE_RETRANSMIT_FREQUENCY                           "migrate.retransmit_frequency"
#define NR_REG_MIGRATE_RETRIES                                        "migrate.retries"
#define NR_MIGRATE_LOCATION_NUMBER_MIN                                0
#define NR_MIGRATE_LOCATION_NUMBER_MAX                                255

#define NR_REG_REVELATION_ENABLE                                      "revelation.enabled"
#define NR_REG_REVELATION_MAX_PER_HOUR                                "revelation.max_per_hour"
#define NR_REG_REVELATION_MAX_PER_HOUR_PER_PORTAL                     "revelation.max_per_hour_per_portal"

/* Appliance-only: */
#define NR_REG_SNMP_ENABLED                                           "snmp.enabled"
#define NR_REG_CLOCK_TIMEZONE                                         "clock.timezone"

#endif

