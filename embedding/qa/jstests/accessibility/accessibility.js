//********************* Cookie functions **********************************

function createCookie(name,value,days)
{
  if (days)
  {
    var date = new Date();
    date.setTime(date.getTime()+(days*24*60*60*1000));
    var expires = "; expires="+date.toGMTString();
  }
  else expires = "";

  document.cookie = name+"="+value+expires+"; path=/";
}

function readCookie(name)
{
  var nameEQ = name + "=";
  var ca = document.cookie.split(';');
  for(var i=0;i < ca.length;i++)
  {
    var c = ca[i];
    while (c.charAt(0)==' ')
      c = c.substring(1,c.length);
    if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
  }
  return null;
}

function eraseCookie(name)
{
  createCookie(name,"",-1);
}

//********************* End of cookie functions ****************************

//************************ General functions *******************************

function displayResults(results)
{
  document.results.textarea.value = results;
  if (top.name == "testWindow")
  {
    fixSubmit();
  }
  else
  {
    document.write(document.results.textarea.value);
  }
}

//********************* End of General functions ***************************

//************** functions for nsIAccessibleService interface **************

function getAccessibleNode(startNode)
{
  var accessibleService = null;
  var accessibleNode = null;
  try{
   netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
   netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

   accessibleService = Components.classes["@mozilla.org/accessibilityService;1"].createInstance();
   accessibleService = accessibleService.QueryInterface(Components.interfaces.nsIAccessibilityService);
  }
  catch(e){
   alert("Error getting accessibility service");
  }

  try{
   netscape.security.PrivilegeManager.enablePrivilege("UniversalBrowserRead");
   netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

   accessibleNode = accessibleService.getAccessibleFor(startNode);
   return accessibleNode;
  }
  catch(e){
   return "Exception";
  }
}

//************** End of functions for nsIAccessibleService interface ***********

//**************** functions of nsIAccessible Interface ************************

// This function will test the attribute accName.
function getName(accNode)
{
 try{
  return accNode.accName;
 }
 catch(e){
  return(e);
 }
}

// This function will test the attribute accRole.
function getRole(accNode)
{
 try{
  return accNode.accRole;
 }
 catch(e){
  return(e);
 }
}

// This function will test the attribute accState.
function getState(accNode)
{
 try{
  return accNode.accState;
 }
 catch(e){
  return(e);
 }
}

// This function will test the attribute accValue.
function getValue(accNode)
{
  try{
   return accNode.accValue;
  }
  catch(e){
    return(e);
  }
}

//****************** End of functions of nsIAccessible Interface *************