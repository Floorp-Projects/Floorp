# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#  
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#  
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 

var _elementIDs = ["networkProxyType",
                    "networkProxyFTP", "networkProxyFTP_Port",
                    "networkProxyGopher", "networkProxyGopher_Port",
                    "networkProxyHTTP", "networkProxyHTTP_Port", 
                    "networkProxySOCKS", "networkProxySOCKS_Port",
                    "networkProxySOCKSVersion",
                    "networkProxySSL", "networkProxySSL_Port", 
                    "networkProxyNone", "networkProxyAutoconfigURL", "shareAllProxies"];

function Startup()
{
  DoEnabling();
}

function DoEnabling()
{
  var i;
  var ftp = document.getElementById("networkProxyFTP");
  var ftpPort = document.getElementById("networkProxyFTP_Port");
  var gopher = document.getElementById("networkProxyGopher");
  var gopherPort = document.getElementById("networkProxyGopher_Port");
  var http = document.getElementById("networkProxyHTTP");
  var httpPort = document.getElementById("networkProxyHTTP_Port");
  var socks = document.getElementById("networkProxySOCKS");
  var socksPort = document.getElementById("networkProxySOCKS_Port");
  var socksVersion = document.getElementById("networkProxySOCKSVersion");
  var socksVersion4 = document.getElementById("networkProxySOCKSVersion4");
  var socksVersion5 = document.getElementById("networkProxySOCKSVersion5");
  var ssl = document.getElementById("networkProxySSL");
  var sslPort = document.getElementById("networkProxySSL_Port");
  var noProxy = document.getElementById("networkProxyNone");
  var autoURL = document.getElementById("networkProxyAutoconfigURL");
  var autoReload = document.getElementById("autoReload");
  var shareAllProxies = document.getElementById("shareAllProxies");

  // convenience arrays
  var manual = [ftp, ftpPort, gopher, gopherPort, http, httpPort, socks, socksPort, socksVersion, socksVersion4, socksVersion5, ssl, sslPort, noProxy, shareAllProxies];
  var manual2 = [http, httpPort, noProxy, shareAllProxies];
  var auto = [autoURL, autoReload];

  // radio buttons
  var radiogroup = document.getElementById("networkProxyType");

  switch ( radiogroup.value ) {
    case "0":
    case "4":
      for (i = 0; i < manual.length; i++)
        manual[i].setAttribute( "disabled", "true" );
      for (i = 0; i < auto.length; i++)
        auto[i].setAttribute( "disabled", "true" );
      break;
    case "1":
      for (i = 0; i < auto.length; i++)
        auto[i].setAttribute( "disabled", "true" );
      if (!radiogroup.disabled && !shareAllProxies.checked) {
        for (i = 0; i < manual.length; i++) {
           prefstring = manual[i].getAttribute( "prefstring" );
           if (!window.opener.top.hPrefWindow.getPrefIsLocked(prefstring))
             manual[i].removeAttribute( "disabled" );
        }
      } else {
        for (i = 0; i < manual.length; i++)
          manual[i].setAttribute("disabled", "true");
        for (i = 0; i < manual2.length; i++) {
           prefstring = manual2[i].getAttribute( "prefstring" );
           if (!window.opener.top.hPrefWindow.getPrefIsLocked(prefstring))
             manual2[i].removeAttribute( "disabled" );
        }
      }
      break;
    case "2":
    default:
      for (i = 0; i < manual.length; i++)
        manual[i].setAttribute("disabled", "true");
      if (!radiogroup.disabled)
        for (i = 0; i < auto.length; i++)
          auto[i].removeAttribute("disabled");
      break;
  }
}

const nsIProtocolProxyService = Components.interfaces.nsIProtocolProxyService;
const kPROTPROX_CID = '{e9b301c0-e0e4-11D3-a1a8-0050041caf44}';

function ReloadPAC() 
{
  var autoURL = document.getElementById("networkProxyAutoconfigURL");
  var pps = Components.classesByID[kPROTPROX_CID]
                       .getService(nsIProtocolProxyService);
  pps.configureFromPAC(autoURL.value);
}   

function onConnectionsDialogOK()
{
  var autoURL = document.getElementById("networkProxyAutoconfigURL");
  var URIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                           .getService(Components.interfaces.nsIURIFixup);
  try {
    var fixedUpURI = URIFixup.createFixupURI(autoURL.value, 0);
    autoURL.value = fixedUpURI.spec;
  }
  catch(ex) {
  }

  window.opener.top.hPrefWindow.wsm.savePageData(window.location.href, window);
  
  return true;
}

var oldUrls = ["","",""];
var oldPorts = ["0","0","0"];

function toggleProxySettings()
{
  var http = document.getElementById("networkProxyHTTP");
  var httpPort = document.getElementById("networkProxyHTTP_Port");
  var ftp = document.getElementById("networkProxyFTP");
  var ftpPort = document.getElementById("networkProxyFTP_Port");
  var gopher = document.getElementById("networkProxyGopher");
  var gopherPort = document.getElementById("networkProxyGopher_Port");
  var ssl = document.getElementById("networkProxySSL");
  var sslPort = document.getElementById("networkProxySSL_Port");
  var socks = document.getElementById("networkProxySOCKS");
  var socksPort = document.getElementById("networkProxySOCKS_Port");
  var socksVersion = document.getElementById("networkProxySOCKSVersion");
  var socksVersion4 = document.getElementById("networkProxySOCKSVersion4");
  var socksVersion5 = document.getElementById("networkProxySOCKSVersion5");
  
  // arrays
  var urls = [ftp,gopher,ssl];
  var ports = [ftpPort,gopherPort,sslPort];
  var allFields = [ftp,gopher,ssl,ftpPort,gopherPort,sslPort,socks,socksPort,socksVersion,socksVersion4,socksVersion5];

  if (document.getElementById("shareAllProxies").checked) {
    for (i = 0; i < allFields.length; i++)
      allFields[i].setAttribute("disabled", "true");
    for (i = 0; i < urls.length; i++) {
      oldUrls[i] = urls[i].value;
      prefstring = urls[i].getAttribute("prefstring");
      if (!window.opener.top.hPrefWindow.getPrefIsLocked(prefstring))
        urls[i].value = http.value;
    }
    for (i = 0; i < ports.length; i++) {
      oldPorts[i] = ports[i].value;
      prefstring = ports[i].getAttribute("prefstring");
      if (!window.opener.top.hPrefWindow.getPrefIsLocked(prefstring))
        ports[i].value = httpPort.value;
    }
  } else {
    for (i = 0; i < allFields.length; i++) {
      prefstring = allFields[i].getAttribute("prefstring");
      if (!window.opener.top.hPrefWindow.getPrefIsLocked(prefstring))
        allFields[i].removeAttribute("disabled");
    }
    for (i = 0; i < urls.length; i++) {
      prefstring = urls[i].getAttribute("prefstring");
      if (!window.opener.top.hPrefWindow.getPrefIsLocked(prefstring))
        urls[i].value = oldUrls[i];
    }
    for (i = 0; i < ports.length; i++) {
      prefstring = ports[i].getAttribute("prefstring");
      if (!window.opener.top.hPrefWindow.getPrefIsLocked(prefstring))
        ports[i].value = oldPorts[i];
    }
  }
}

function copyProxySettings()
{
  if (!document.getElementById("shareAllProxies").checked)
    return;
  
  var http = document.getElementById("networkProxyHTTP");
  var httpPort = document.getElementById("networkProxyHTTP_Port");
  var ftp = document.getElementById("networkProxyFTP");
  var ftpPort = document.getElementById("networkProxyFTP_Port");
  var gopher = document.getElementById("networkProxyGopher");
  var gopherPort = document.getElementById("networkProxyGopher_Port");
  var ssl = document.getElementById("networkProxySSL");
  var sslPort = document.getElementById("networkProxySSL_Port");
  
  var urls = [ftp,gopher,ssl];
  var ports = [ftpPort,gopherPort,sslPort];
  
  for (i = 0; i < urls.length; i++) {
    prefstring = urls[i].getAttribute("prefstring");
    if (!window.opener.top.hPrefWindow.getPrefIsLocked(prefstring))
      urls[i].value = http.value;
  }
  for (i = 0; i < ports.length; i++) {
    prefstring = ports[i].getAttribute("prefstring");
    if (!window.opener.top.hPrefWindow.getPrefIsLocked(prefstring))
      ports[i].value = httpPort.value;
  }
}
