/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
  * This dictionnary holds the parameters sent to the wifi service.
  */
dictionary WifiCommandOptions
{
  long      id = 0;       // opaque id.
  DOMString cmd = "";     // the command name.
  DOMString request;      // for "command"
};

/**
  * This dictionnary holds the parameters sent back to WifiWorker.js
  */
dictionary WifiResultOptions
{
  long      id = 0;             // opaque id.
  long      status = 0;         // the return status of the command.
                                // Used by most commands.
  DOMString reply = "";         // for "command".
};


/**
  * This dictionary holds the callback parameter sent back from WifiCertService
  * to WifiWorker, and should only be passed around in chrome process.
  */
dictionary WifiCertServiceResultOptions
{
  long            id = 0;         // request id in WifiWorker.
  long            status = 0;     // error code of the request, 0 indicates success.
  unsigned short  usageFlag = 0;  // usage flag of certificate, the flag is defined
                                  // in nsIWifiCertService.idl
  DOMString       nickname = "";  // nickname of certificate of the request.
};
