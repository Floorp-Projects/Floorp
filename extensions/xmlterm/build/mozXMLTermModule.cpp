/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 2000 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of
 * those above. If you wish to allow use of your version of this
 * file only under the terms of the GPL and not to allow
 * others to use your version of this file under the MPL, indicate
 * your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "nsIGenericFactory.h"
#include "mozLineTerm.h"
#include "mozXMLTermShell.h"
#include "mozXMLTerminal.h"
#include "mozXMLTermStream.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(mozXMLTerminal)
NS_GENERIC_FACTORY_CONSTRUCTOR(mozXMLTermStream)

// CIDs implemented by module
static nsModuleComponentInfo components[] =
{
    { MOZLINETERM_CLASSNAME,
      MOZLINETERM_CID,
      MOZLINETERM_CONTRACTID,
      mozLineTerm::Create,
    },

    { MOZXMLTERMSHELL_CLASSNAME,
      MOZXMLTERMSHELL_CID,
      MOZXMLTERMSHELL_CONTRACTID,
      mozXMLTermShell::Create,
      mozXMLTermShell::RegisterProc,
      mozXMLTermShell::UnregisterProc 
    },

    { MOZXMLTERMINAL_CLASSNAME,
      MOZXMLTERMINAL_CID,
      MOZXMLTERMINAL_CONTRACTID,
      mozXMLTerminalConstructor,
    },

    { MOZXMLTERMSTREAM_CLASSNAME,
      MOZXMLTERMSTREAM_CID,
      MOZXMLTERMSTREAM_CONTRACTID,
      mozXMLTermStreamConstructor,
    },

};

// Module entry point
NS_IMPL_NSGETMODULE(mozXMLTermModule, components)
