// Error url MUST be formatted like this:
//   chrome://neterror.xhtml?e=error&u=url&d=desc

function getErrorCode()
{
  var url = document.location.href;
  var error = url.search(/e\=/);
  var duffUrl = url.search(/\&u\=/);
  return decodeURIComponent(url.slice(error + 2, duffUrl));
}

function getDuffUrl()
{
  var url = document.location.href;
  var duffUrl = url.search(/u\=/);
  var desc = url.search(/\&d\=/);
  return decodeURIComponent(url.slice(duffUrl + 2, desc));
}

function getDescription()
{
  var url = document.location.href;
  var desc = url.search(/d\=/);
  return decodeURIComponent(url.slice(desc + 2));
}

function retryThis()
{
  var duffUrl = getDuffUrl();
  document.location.href = duffUrl;
}

function fillIn()
{
  var err = getErrorCode();
  var duffUrl = getDuffUrl();
  var i;

  // Fill in the title
  var et = document.getElementById("et_" + err);
  if (et) {
    et.className = "et_visible";
  }


//  for (i = 0; i < t.childNodes.length; i++)
//  {
//    var n = t.childNodes.item(i);
//    if (n.nodeType == Node.TEXT_NODE)
//    {
//      n.nodeValue = titleText;
//      break;
//    }
//  }

  // Fill in the short description
  var sd = document.getElementById("shortDesc");
  for (i = 0; i < sd.childNodes.length; i++)
  {
    var n = sd.childNodes.item(i);
    if (n.nodeType == Node.TEXT_NODE)
    {
      n.nodeValue = getDescription();
      break;
    }
  }

  // Long description
  var ld = document.getElementById("ld_" + err);
  if (ld) {
    ld.className = "ld_visible";
  }
}
