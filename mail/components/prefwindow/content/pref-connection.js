var _elementIDs = ["networkProxyType",
                   "networkProxyHTTP", "networkProxyHTTP_Port", 
                   "networkProxySOCKS", "networkProxySOCKS_Port",
                   "networkProxySOCKSVersion",
                   "networkProxySSL", "networkProxySSL_Port", 
                   "networkProxyNone", "networkProxyAutoconfigURL" ];
function Startup()
{
  DoEnabling();
}

// proxy config support
function DoEnabling()
{
  var i;
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

  // convenience arrays
  var manual = [http, httpPort, socks, socksPort, socksVersion, socksVersion4, socksVersion5, ssl, sslPort, noProxy];
  var auto = [autoURL, autoReload];

  // radio buttons
  var radiogroup = document.getElementById("networkProxyType");

  switch ( radiogroup.value ) {
    case "0":
      for (i = 0; i < manual.length; i++)
        manual[i].setAttribute( "disabled", "true" );
      for (i = 0; i < auto.length; i++)
        auto[i].setAttribute( "disabled", "true" );
      break;
    case "1":
      for (i = 0; i < auto.length; i++)
        auto[i].setAttribute( "disabled", "true" );
      if (!radiogroup.disabled)
        for (i = 0; i < manual.length; i++)
          manual[i].removeAttribute( "disabled" );
      break;
    case "2":
    default:
      for (i = 0; i < manual.length; i++)
        manual[i].setAttribute( "disabled", "true" );
      if (!radiogroup.disabled)
        for (i = 0; i < auto.length; i++)
          auto[i].removeAttribute( "disabled" );
      break;
  }
}

const nsIProtocolProxyService = Components.interfaces.nsIProtocolProxyService;
const kPROTPROX_CID = '{e9b301c0-e0e4-11D3-a1a8-0050041caf44}';

function ReloadPAC() {
  var autoURL = document.getElementById("networkProxyAutoconfigURL");
  var pps = Components.classesByID[kPROTPROX_CID]
                       .getService(nsIProtocolProxyService);
  pps.configureFromPAC(autoURL.value);
}  

function onConnectionsDialogOK()
{
  window.opener.top.hPrefWindow.wsm.savePageData(window.location.href, window); 
  return true;
}
