/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is ChatZilla
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Robert Ginda, <rginda@netscape.com>, original author
 *
 */

function initMessages()
{
    var path = "chrome://chatzilla/locale/chatzilla.properties";
    
    client.messageManager = new MessageManager();
    client.defaultBundle = client.messageManager.addBundle(path);

    client.name = MSG_CLIENT_NAME;
    client.responseCodeMap =
        {
            "HELLO": MSG_RSP_HELLO,
            "HELP" : MSG_RSP_HELP,
            "USAGE": MSG_RSP_USAGE,
            "ERROR": MSG_RSP_ERROR,
            "WARNING": MSG_RSP_WARN,
            "INFO": MSG_RSP_INFO,
            "EVAL-IN": MSG_RSP_EVIN,
            "EVAL_-OUT": MSG_RSP_EVOUT,
            "JOIN": "-->|",
            "PART": "<--|",
            "QUIT": "|<--",
            "NICK": "=-=",
            "TOPIC": "=-=",
            "KICK": "=-=",
            "MODE": "=-=",
            "END_STATUS": "---",
            "315": "---", /* end of WHO */
            "318": "---", /* end of WHOIS */
            "366": "---", /* end of NAMES */
            "376": "---"  /* end of MOTD */
        };
}

function checkCharset(charset)
{
    return client.messageManager.checkCharset(charset);
}

function toUnicode (msg, charsetOrView)
{
    if (!msg)
        return msg;

    var charset;
    if (typeof charsetOrView == "object")
        charset = charsetOrView.prefs["charset"];
    else if (typeof charsetOrView == "string")
        charset = charsetOrView;
    else
        charset = client.currentObject.prefs["charset"];

    return client.messageManager.toUnicode(msg, charset);
}

function fromUnicode (msg, charsetOrView)
{
    if (!msg)
        return msg;

    var charset;
    if (typeof charsetOrView == "object")
        charset = charsetOrView.prefs["charset"];
    else if (typeof charsetOrView == "string")
        charset = charsetOrView;
    else
        charset = client.currentObject.prefs["charset"];

    return client.messageManager.fromUnicode(msg, charset);
}

function getMsg(msgName, params, deflt)
{
    return client.messageManager.getMsg(msgName, params, deflt);
}

function getMsgFrom(bundle, msgName, params, deflt)
{
    return client.messageManager.getMsgFrom(bundle, msgName, params, deflt);
}

/* message types, don't localize */
const MT_ATTENTION = "ATTENTION";
const MT_ERROR     = "ERROR";
const MT_HELLO     = "HELLO";
const MT_HELP      = "HELP";
const MT_MODE      = "MODE";
const MT_WARN      = "WARN";
const MT_INFO      = "INFO";
const MT_USAGE     = "USAGE";
const MT_STATUS    = "STATUS";
const MT_EVALIN    = "EVAL-IN";
const MT_EVALOUT   = "EVAL-OUT";

