/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ashish Bhatt <ashishbhatt@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


const IO_SERVICE_CTRID = 
	"@mozilla.org/network/io-service;1" ;
const nsIFileProtocolHandler = 
 	Components.interfaces.nsIFileProtocolHandler ;
const nsIIOService = 
	Components.interfaces.nsIIOService ;

function FileProtocolHandler()
{
    try {
	    netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	    ioservice = Components.classes[IO_SERVICE_CTRID].getService(nsIIOService);

	    
	    if(!ioservice)
	       this.exception = "Unable to get nsIIOService" ;
	          
	    this.protocolhandler = ioservice.getProtocolHandler("file").QueryInterface(nsIFileProtocolHandler) ;

	    if (!this.protocolhandler)
	       this.exception = "Unable to get nsIFileProtocolHandler" ;
	      
	   return this ;
     }
     catch(e){
	this.exception = e ;
     }
     
	 
   
}

FileProtocolHandler.prototype.returnvalue = null;
FileProtocolHandler.prototype.success = null;
FileProtocolHandler.prototype.exception = null;
FileProtocolHandler.prototype.protocolhandler = null ;


FileProtocolHandler.prototype.newFileURI = function (aFile)
{
  /**
  * This method constructs a new file URI 
  *
  * @param aFile nsIFile
  * @return reference to a new nsIURI object
  *
  *  nsIURI newFileURI(in nsIFile aFile);
  */
  
  try {
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	 this.returnvalue = this.protocolhandler.newFileURI(aFile)
	  if (this.returnvalue)
	    this.success =  true ;
	  else
	    this.success =  false ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }

}


FileProtocolHandler.prototype.getURLSpecFromFile = function (aFile)
{
 /**
  * Converts the nsIFile to the corresponding URL string.  NOTE: under
  * some platforms this is a lossy conversion (e.g., Mac Carbon build).
  * If the nsIFile is a local file, then the result will be a file://
  * URL string.
  *
  * The resulting string may contain URL-escaped characters.
  *
  * AUTF8String getURLSpecFromFile(in nsIFile file);
  */
  try {
	 
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	 netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
	
	 this.returnvalue = this.protocolhandler.getURLSpecFromFile(aFile)
	  if (this.returnvalue)
	    this.success =  true ;
	  else
	    this.success =  false ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }
 
 }

FileProtocolHandler.prototype.getFileFromURLSpec = function (aUrl)
{
 /**
  * Converts the URL string into the corresponding nsIFile if possible.
  * A local file will be created if the URL string begins with file://.
  *
  * nsIFile getFileFromURLSpec(in AUTF8String url);
  */
  
  try {
	  netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
	  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

	  this.returnvalue = this.protocolhandler.getFileFromURLSpec(aUrl)
	  if (this.returnvalue)
	    this.success =  true ;
	  else
	    this.success =  false ;
  }
  catch(e){
    this.success =  false ;
    this.exception =  e ;
  }
 
}

