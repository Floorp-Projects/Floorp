// Error url MUST be formatted like this:
//   chrome://neterror.xhtml?e=error&u=url&d=desc

function getErrorCode()
{
  url = document.location.href;
  error = url.search(/e\=/);
  duffUrl = url.search(/\&u\=/);
  return unescape(url.slice(error + 2, duffUrl));
}

function getDuffUrl()
{
  url = document.location.href;
  duffUrl = url.search(/u\=/);
  desc = url.search(/\&d\=/);
  return unescape(url.slice(duffUrl + 2, desc));
}

function getDescription()
{
  url = document.location.href;
  desc = url.search(/d\=/);
  return unescape(url.slice(desc + 2));
}

function retryThis()
{
  var duffUrl = getDuffUrl();
  document.location.href = duffUrl;
}

function searchThis()
{
  searchForThis(getDuffUrl());
}

function searchForThis(url)
{
  document.location.href = "http://www.google.com/search?q=" + url;
}

function fillIn()
{
  var err = getErrorCode();
  var duffUrl = getDuffUrl();
  var i;

  // Fill in the title
  var titleText =  "&" + err + ".title;";
  document.title = titleText;

// document.getElementById("title").innerHTML = "<p>bbb</p>";

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
  var d = document.getElementById(err);
  d.setAttribute("style", "display: block;");
}
