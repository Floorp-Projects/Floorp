var popup;

function OpenWindow()
{
log({},">>> OpenWindow");
	popup = window.open("","Test");

	var output = "<html>";
	
	output+="<body>";
	output+="<form>"
	output+="<input id='popupText1' type='text' onfocus='opener.log(event)' onblur='opener.log(event)'>";
	output+="</form>"
	output+="</body>";
	output+="</html>";
	
	popup.document.open();
	popup.document.write(output);
	popup.document.close();

        popup.document.onclick=function (event) { log(event,"popup-doc") };
        popup.document.onfocus=function (event) { log(event,"popup-doc") };
        popup.document.onblur=function (event) { log(event,"popup-doc") };
        popup.document.onchange=function (event) { log(event,"popup-doc") };

	var e = popup.document.getElementById('popupText1');
	popup.focus();
	e.focus();
        is(popup.document.activeElement, e, "input element in popup should be focused");
log({},"<<< OpenWindow");
}

var result;

function log(event,message) {
  if (event&&event.eventPhase==3) return;
  e = event.currentTarget||event.target||event.srcElement;
  var id = e?(e.id?e.id:e.name?e.name:e.value?e.value:''):'';
  if (id) id = '(' + id + ')';
  result +=
    (e?(e.tagName?e.tagName:''):' ')+id+': '+
    (event.type?event.type:'')+' '+
    (message?message:'') + '\n';
}

document.onclick=function (event) { log(event,"top-doc") };
document.onfocus=function (event) { log(event,"top-doc") };
document.onblur=function (event) { log(event,"top-doc") };
document.onchange=function (event) { log(event,"top-doc") };

function doTest1_rest2(expectedEventLog,focusAfterCloseId) {
  try {
    is(document.activeElement, document.getElementById(focusAfterCloseId), "wrong element is focused after popup was closed");
    is(result, expectedEventLog, "unexpected events");
    SimpleTest.finish();
  } catch(e) {
    if (popup)
      popup.close();
    throw e;
  }
}
function doTest1_rest1(expectedEventLog,focusAfterCloseId) {
  try {
    synthesizeKey("V", {}, popup);
    synthesizeKey("A", {}, popup);
    synthesizeKey("L", {}, popup);
    is(popup.document.getElementById('popupText1').value, "VAL", "input element in popup did not accept input");

    var p = popup;
    popup = null;
    p.close();

    SimpleTest.waitForFocus(function () { doTest1_rest2(expectedEventLog,focusAfterCloseId); }, window);
  } catch(e) {
    if (popup)
      popup.close();
    throw e;
  }
}

function doTest1(expectedEventLog,focusAfterCloseId) {
  try {
    var select1 = document.getElementById('Select1');
    select1.focus();
    is(document.activeElement, select1, "select element should be focused");
    synthesizeKey("VK_DOWN",{});
    synthesizeKey("VK_TAB", {});
    SimpleTest.waitForFocus(function () { doTest1_rest1(expectedEventLog,focusAfterCloseId); }, popup);

  } catch(e) {
    if (popup)
      popup.close();
    throw e;
  }
}

function setPrefAndDoTest(expectedEventLog,focusAfterCloseId,prefValue) {
  var select1 = document.getElementById('Select1');
  select1.blur();
  result = "";
  log({},"Test with browser.link.open_newwindow = "+prefValue);
   SpecialPowers.pushPrefEnv({"set": [['browser.link.open_newwindow', prefValue]]}, function() {
     doTest1(expectedEventLog,focusAfterCloseId);
   });
}
