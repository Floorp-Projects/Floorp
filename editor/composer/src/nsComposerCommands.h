/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   
 */

#ifndef nsComposerCommands_h_
#define nsComposerCommands_h_


#include "nsEditorCommands.h"

// virtual base class for commands that need to save and update state
class nsBaseStateUpdatingCommand : public nsBaseCommand,
                                   public nsIStateUpdatingControllerCommand
{
public:

              nsBaseStateUpdatingCommand(const char* aTagName);
  virtual     ~nsBaseStateUpdatingCommand();
    
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSICONTROLLERCOMMAND
  NS_DECL_NSISTATEUPDATINGCONTROLLERCOMMAND

protected:

  // get the current state (on or off) for this style or block format
  virtual nsresult  GetCurrentState(nsIEditorShell *aEditorShell, const char* aTagName, PRBool& outStateSet) = 0;
  
  // add/remove the style
  virtual nsresult  ToggleState(nsIEditorShell *aEditorShell, const char* aTagName) = 0;

protected:

  const char* mTagName;
  const char* mAttributeName;
  
};


// shared class for the various style updating commands
class nsStyleUpdatingCommand : public nsBaseStateUpdatingCommand
{
public:

            nsStyleUpdatingCommand(const char* aTagName);
           
protected:

  // get the current state (on or off) for this style or block format
  virtual nsresult  GetCurrentState(nsIEditorShell *aEditorShell, const char* aTagName, PRBool& outStateSet);
  
  // add/remove the style
  virtual nsresult  ToggleState(nsIEditorShell *aEditorShell, const char* aTagName);
    
};


class nsListCommand : public nsBaseStateUpdatingCommand
{
public:

            nsListCommand(const char* aTagName);

protected:
  
  // get the current state (on or off) for this style or block format
  virtual nsresult  GetCurrentState(nsIEditorShell *aEditorShell, const char* aTagName, PRBool& outStateSet);
  
  // add/remove the style
  virtual nsresult  ToggleState(nsIEditorShell *aEditorShell, const char* aTagName);
};


// composer commands
NS_DECL_EDITOR_COMMAND(nsAlwaysEnabledCommands)

NS_DECL_EDITOR_COMMAND(nsCloseCommand)
NS_DECL_EDITOR_COMMAND(nsPrintingCommands)

// Generic commands


// File menu
NS_DECL_EDITOR_COMMAND(nsNewCommands)   // handles 'new' anything


NS_DECL_EDITOR_COMMAND(nsSaveCommand)
NS_DECL_EDITOR_COMMAND(nsSaveAsCommand)



// Edit menu
NS_DECL_EDITOR_COMMAND(nsPasteQuotationCommand)

// Block transformations
NS_DECL_EDITOR_COMMAND(nsIndentCommand)
NS_DECL_EDITOR_COMMAND(nsOutdentCommand)

NS_DECL_EDITOR_COMMAND(nsParagraphStateCommand)
NS_DECL_EDITOR_COMMAND(nsAlignCommand)
NS_DECL_EDITOR_COMMAND(nsRemoveStylesCommand)
NS_DECL_EDITOR_COMMAND(nsIncreaseFontSizeCommand)
NS_DECL_EDITOR_COMMAND(nsDecreaseFontSizeCommand)



#endif // nsComposerCommands_h_
