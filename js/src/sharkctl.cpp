/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * Copyright (C) 2010 Patrick Walton <pcwalton@mimiga.net>
 *
 * Contributor(s):
 *      Andreas Gal <gal@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef __APPLE__

#include "sharkctl.h"

#include "sharkctl.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "jsutil.h"

#define SHARK_MSG_ACQUIRE   0x29a
#define SHARK_MSG_RELEASE   0x29b
#define SHARK_MSG_STOP      0x29c
#define SHARK_MSG_START     0x29d

#define RECV_SIZEOF(ty) offsetof(ty, out)

// Private API in libSystem.dylib
extern "C" void bootstrap_look_up(mach_port_t special_port, const char *name,
                                  mach_port_t *dest_port);

struct chud_client_acquire_msg {
    mach_msg_header_t hdr;
    uint32_t unk0;          // always 0
    uint32_t unk1;          // always 1
    uint32_t pid;
    uint32_t out[2];
};

struct chud_client_start_msg {
    mach_msg_header_t hdr;
    uint32_t unk0;          // always 1
    uint32_t name0;
    uint32_t arg2;          // always 6
    uint8_t unk1;           // always 0
    uint8_t unk2;           // always 1
    uint8_t unk3;           // uninitialized
    uint8_t unk4;           // always 1
    uint32_t unk5;          // always 0
    uint32_t unk6;          // always 1
    uint32_t name1;         // same as name0
};

struct chud_client_stop_msg {
    mach_msg_header_t hdr;
    uint32_t out[5];
};

struct chud_client_release_msg {
    mach_msg_header_t hdr;
    uint32_t unk0;          // always 0
    uint32_t unk1;          // always 1
    uint32_t pid;
    uint32_t out[2];
};

static mach_port_t
CreatePort(void)
{
    mach_port_t bootstrap_port, shark_port = 0;
    task_get_special_port(mach_task_self(), TASK_BOOTSTRAP_PORT,
                          &bootstrap_port);
    bootstrap_look_up(bootstrap_port, "CHUD_IPC", &shark_port);
    return shark_port;
}

static mach_msg_return_t
Connect(mach_port_t shark_port)
{
    mach_port_t reply_port = mig_get_reply_port();

    struct chud_client_acquire_msg msg;
    msg.hdr.msgh_bits = 0x1513;
    msg.hdr.msgh_size = sizeof(mach_msg_header_t);
    msg.hdr.msgh_remote_port = shark_port;
    msg.hdr.msgh_local_port = reply_port;
    msg.hdr.msgh_reserved = 0;
    msg.hdr.msgh_id = SHARK_MSG_ACQUIRE;
    msg.unk0 = 0;
    msg.unk1 = 1;
    msg.pid = getpid();

    JS_ASSERT(RECV_SIZEOF(struct chud_client_acquire_msg) == 0x24);
    JS_ASSERT(sizeof(msg) == 0x2c);
    mach_msg_return_t result = mach_msg(&msg.hdr, MACH_SEND_MSG | MACH_RCV_MSG,
                                        RECV_SIZEOF(struct chud_client_acquire_msg),
                                        sizeof(msg), reply_port, 0, 0);
    mig_dealloc_reply_port(reply_port);
    return result;
}

static mach_msg_return_t
Start(mach_port_t shark_port, uint32_t name)
{
    mach_port_t reply_port = mig_get_reply_port();

    struct chud_client_start_msg msg;
    msg.hdr.msgh_bits = 0x80001513;
    msg.hdr.msgh_size = sizeof(mach_msg_header_t);
    msg.hdr.msgh_remote_port = shark_port;
    msg.hdr.msgh_local_port = reply_port;
    msg.hdr.msgh_reserved = 0;
    msg.hdr.msgh_id = SHARK_MSG_START;
    msg.unk0 = 1;
    msg.name0 = name;
    msg.arg2 = 6;
    msg.unk1 = 0;
    msg.unk2 = 1;
    msg.unk3 = 0;
    msg.unk4 = 1;
    msg.unk5 = 0;
    msg.unk6 = 1;
    msg.name1 = name;

    JS_ASSERT(sizeof(msg) == 0x34);
    mach_msg_return_t result = mach_msg(&msg.hdr, MACH_SEND_MSG | MACH_RCV_MSG,
                                        sizeof(msg), 0x30, reply_port, 0, 0);
    mig_dealloc_reply_port(reply_port);
    return result;
}

mach_msg_return_t
Stop(mach_port_t shark_port)
{
    mach_port_t reply_port = mig_get_reply_port();

    struct chud_client_stop_msg msg;
    msg.hdr.msgh_bits = 0x1513;
    msg.hdr.msgh_size = sizeof(mach_msg_header_t);
    msg.hdr.msgh_remote_port = shark_port;
    msg.hdr.msgh_local_port = reply_port;
    msg.hdr.msgh_reserved = 0;
    msg.hdr.msgh_id = SHARK_MSG_STOP;

    JS_ASSERT(RECV_SIZEOF(struct chud_client_stop_msg) == 0x18);
    JS_ASSERT(sizeof(msg) == 0x2c);
    mach_msg_return_t result = mach_msg(&msg.hdr, MACH_SEND_MSG | MACH_RCV_MSG,
                                        RECV_SIZEOF(struct chud_client_stop_msg),
                                        sizeof(msg), reply_port, 0, 0);
    mig_dealloc_reply_port(reply_port);
    return result;
}

static mach_msg_return_t
Disconnect(mach_port_t shark_port)
{
    mach_port_t reply_port = mig_get_reply_port();

    struct chud_client_release_msg msg;
    msg.hdr.msgh_bits = 0x1513;
    msg.hdr.msgh_size = sizeof(mach_msg_header_t);
    msg.hdr.msgh_remote_port = shark_port;
    msg.hdr.msgh_local_port = reply_port;
    msg.hdr.msgh_reserved = 0;
    msg.hdr.msgh_id = SHARK_MSG_RELEASE;
    msg.unk0 = 0;
    msg.unk1 = 1;
    msg.pid = getpid();

    JS_ASSERT(RECV_SIZEOF(struct chud_client_release_msg) == 0x24);
    JS_ASSERT(sizeof(msg) == 0x2c);
    mach_msg_return_t result = mach_msg(&msg.hdr, MACH_SEND_MSG | MACH_RCV_MSG,
                                        RECV_SIZEOF(struct chud_client_release_msg),
                                        sizeof(msg), reply_port, 0, 0);
    mig_dealloc_reply_port(reply_port);
    return result;
}

static mach_port_t shark_port = 0;
static bool connected = false;
static bool running = false;

namespace Shark {

bool
Start()
{
    if (!shark_port && !(shark_port = CreatePort()))
        return false;
    if (!connected && Connect(shark_port))
        return false;
    connected = true;
    if (!running && ::Start(shark_port, 0xdeadbeef))
        return false;
    return running = true;
};

void
Stop()
{
    if (!shark_port || !connected)
        return;
    ::Stop(shark_port);
    running = false;
    Disconnect(shark_port);
    connected = false;
}

}

#endif
