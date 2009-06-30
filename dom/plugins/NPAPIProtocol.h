/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Chris Jones <jones.chris.g@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef dom_plugins_NPAPIProtocol_h
#define dom_plugins_NPAPIProtocol_h 1

#include "npapi.h"

#include "IPC/IPCMessageUtils.h"
#include "mozilla/ipc/MessageTypes.h"

namespace mozilla {
namespace plugins {
//-----------------------------------------------------------------------------

class NS_FINAL_CLASS NPAPIProtocol
{
public:
    /**
     * These methods mirror the messages that are sent from
     * Child-->Parent.
     *
     * A class in the parent must implement this interface.
     *
     * The child has access to this interface through an
     * NPAPIProtocolChildImpl instace.
     */
    /*interface*/ class Parent
    {
    protected:
        Parent() { }
        virtual ~Parent() { }
        Parent(const Parent&);
        Parent& operator=(const Parent&);
    };

    /**
     * These methods mirror the messages that are sent from
     * Parent-->Child.
     *
     * A class in the child must implement this interface.
     *
     * The parent has access to this interface through an
     * NPAPIProtocolParentImpl instace.
     */
    /*interface*/ class Child
    {
    public:
        virtual NPError NP_Initialize() = 0;

        virtual NPError NPP_New(const mozilla::ipc::String& aMimeType,
                                /*const NPP::Parent**/ const int& aHandle,
                                const uint16_t& aMode,
                                const mozilla::ipc::StringArray& aNames,
                                const mozilla::ipc::StringArray& aValues) = 0;

        virtual void NPP_Destroy(/*...*/) = 0;

    protected:
        Child() { }
        virtual ~Child() { }
        Child(const Child&);
        Child& operator=(const Child&);
    };

    enum State {
        StateStart = 0,

        StateInitializing = StateStart,
        StateInitialized,
        StateNotScriptable,
        StateScriptable,

        // placeholder
        StateLast
    };

    // FIXME add transition table

private:
    NPAPIProtocol();
    virtual ~NPAPIProtocol() = 0;
    NPAPIProtocol& operator=(const NPAPIProtocol&);
    // FIXME/cjones state to allow dynamic checking?
};

//-----------------------------------------------------------------------------
// Start NPAPI protocol messages
//

// FIXME/cjones: we should generate code for the "Param" serialization and
// deserialization of message parameters, so as to avoid the macro/template
// garbage that impedes debugging.  the amount of generated code should be
// approximately the same, unless we have a lot of messages with the same
// parameters.  e.g., Msg1(int, int), Msg2(int, int) ... it's a trade off
// between code size and debugging ease

// messages that are sent from parent to the child.  a "reply" to a child
// message is sent parent to child (ChildToParentMsg_Reply...)
enum NPAPI_ParentToChildMsgType {
    NPAPI_ParentToChildStart = NPAPI_ParentToChildMsgStart << 12,
    NPAPI_ParentToChildPreStart = (NPAPI_ParentToChildMsgStart << 12) - 1,

    NPAPI_ParentToChildMsg_NP_Initialize__ID,
    NPAPI_ParentToChildMsg_NPP_New__ID,
    NPAPI_ParentToChildMsg_NPP_Destroy__ID,

    NPAPI_ParentToChildEnd
};

class NPAPI_ParentToChildMsg_NP_Initialize : public IPC::Message
{
public:
    enum { ID = NPAPI_ParentToChildMsg_NP_Initialize__ID };
    NPAPI_ParentToChildMsg_NP_Initialize() :
        IPC::Message(MSG_ROUTING_CONTROL, ID, PRIORITY_NORMAL)
    {}
};

class NPAPI_ParentToChildMsg_NPP_New :
        public IPC::MessageWithTuple<
    Tuple5<mozilla::ipc::String, int, uint16_t, mozilla::ipc::StringArray, mozilla::ipc::StringArray> >
{
public:
    enum { ID = NPAPI_ParentToChildMsg_NPP_New__ID };
    NPAPI_ParentToChildMsg_NPP_New(const mozilla::ipc::String& aMimeType,
                                   /*const NPP::Parent*& aHandle,*/
                                   const int& aHandle,
                                   const uint16_t& aMode,
                                   const mozilla::ipc::StringArray& aNames,
                                   const mozilla::ipc::StringArray& aValues) :
        IPC::MessageWithTuple<
        Tuple5<mozilla::ipc::String, int, uint16_t, mozilla::ipc::StringArray, mozilla::ipc::StringArray> >(
            MSG_ROUTING_CONTROL, ID, MakeTuple(aMimeType,
                                               aHandle,
                                               aMode,
                                               aNames,
                                               aValues))
    {}
};

class NPAPI_ParentToChildMsg_NPP_Destroy : public IPC::Message {
public:
    enum { ID = NPAPI_ParentToChildMsg_NPP_Destroy__ID };
    NPAPI_ParentToChildMsg_NPP_Destroy() :
        IPC::Message(MSG_ROUTING_CONTROL, ID, PRIORITY_NORMAL)
    {}
};


// messages from child->parent
enum NPAPI_ChildToParentMsgType {
    NPAPI_ChildToParentStart = NPAPI_ChildToParentMsgStart << 12,
    NPAPI_ChildToParentPreStart = (NPAPI_ChildToParentMsgStart << 12) - 1,

    NPAPI_ChildToParentMsg_Reply_NP_Initialize__ID,

    NPAPI_ChildToParentMsg_Reply_NPP_New__ID,
    NPAPI_ChildToParentMsg_Reply_NPP_Destroy__ID,

    NPAPI_ChildToParentEnd
};

class NPAPI_ChildToParentMsg_Reply_NP_Initialize :
        public IPC::MessageWithTuple<NPError>
{
public:
    enum { ID = NPAPI_ChildToParentMsg_Reply_NP_Initialize__ID };
    NPAPI_ChildToParentMsg_Reply_NP_Initialize(const NPError& arg1) :
        IPC::MessageWithTuple<NPError>(MSG_ROUTING_CONTROL, ID, arg1)
    {}
};

class NPAPI_ChildToParentMsg_Reply_NPP_New :
        public IPC::MessageWithTuple<NPError>
{
public:
    enum { ID = NPAPI_ChildToParentMsg_Reply_NPP_New__ID };
    NPAPI_ChildToParentMsg_Reply_NPP_New(const NPError& arg1) :
        IPC::MessageWithTuple<NPError>(MSG_ROUTING_CONTROL, ID, arg1)
    {}
};

class NPAPI_ChildToParentMsg_Reply_NPP_Destroy : public IPC::Message
{
public:
    enum { ID = NPAPI_ChildToParentMsg_Reply_NPP_Destroy__ID };
    NPAPI_ChildToParentMsg_Reply_NPP_Destroy()
        : IPC::Message(MSG_ROUTING_CONTROL, ID, PRIORITY_NORMAL)
    {}
};


#if 0
// (note to self: see plugin_messages.h for how to define msg with > 6 params)
class FOO_5PARAMS :
        public IPC::MessageWithTuple<
    Tuple5<NPError, int, bool, std::string, short> >
{
public:
    enum { ID = FOO_5PARAMS__ID };
    FOO_5PARAMS(const NPError& arg1,
                const int& arg2,
                const bool& arg3,
                const std::string& arg4,
                const short& arg5) :
        IPC::MessageWithTuple<
        Tuple5<NPError, int, bool, std::string, short> >(
            MSG_ROUTING_CONTROL, ID, MakeTuple(arg1, arg2, arg3, arg4, arg5))
    {}
};
#endif


} // namespace plugins
} // namespace mozilla


#endif  // ifndef dom_plugins_NPAPIProtocol_h
