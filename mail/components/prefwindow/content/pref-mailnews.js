function Startup()
{
  var startupFunc;
  try {
    startupFunc = document.getElementById("mailnewsEnableMapi").getAttribute('startupFunc');
  }
  catch (ex) {
    startupFunc = null;
  }
  if (startupFunc)
    eval(startupFunc);

  StartPageCheck();

  // now do some Notification panel startup work: 

  PlaySoundCheck();

  // if we don't have the alert service, hide the pref UI for using alerts to notify on new mail
  // see bug #158711
  var newMailNotificationAlertUI = document.getElementById("newMailNotificationAlert");
  newMailNotificationAlertUI.hidden = !("@mozilla.org/alerts-service;1" in Components.classes);

#ifdef MOZ_WIDGET_GTK2
  // first check whether GNOME is available.  if it's not, hide the whole
  // default mail/news app section.

  var mapiReg;
  try {
    mapiReg = Components.classes["@mozilla.org/mapiregistry;1"].
                    getService(Components.interfaces.nsIMapiRegistry);
  } catch (e) {}
  if (!mapiReg) {
    document.getElementById("defaultClientBox").hidden = true;
    return;
  }
#endif
}

#ifdef MOZ_WIDGET_GTK2
function checkDefaultMailNow(isNews) {
  var mapiReg = Components.classes["@mozilla.org/mapiregistry;1"]
                 .getService(Components.interfaces.nsIMapiRegistry);

  var brandBundle = document.getElementById("brandBundle");
  var mapiBundle = document.getElementById("mapiBundle");

  var brandShortName = brandBundle.getString("brandRealShortName");
  var promptTitle = mapiBundle.getFormattedString("dialogTitle",
                                                  [brandShortName]);
  var promptMessage;

  var IPS = Components.interfaces.nsIPromptService;
  var prompt = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                  .getService(IPS);

  var isDefault;
  var str;
  if (isNews) {
    isDefault = mapiReg.isDefaultNewsClient;
    str = "News";
  } else {
    isDefault = mapiReg.isDefaultMailClient;
    str = "Mail";
  }

  if (!isDefault) {
    promptMessage = mapiBundle.getFormattedString("setDefault" + str,
                                                  [brandShortName]);

    var rv = prompt.confirmEx(window, promptTitle, promptMessage,
                              (IPS.BUTTON_TITLE_YES * IPS.BUTTON_POS_0) +
                              (IPS.BUTTON_TITLE_NO * IPS.BUTTON_POS_1),
                              null, null, null, null, { });
    if (rv == 0) {
      if (isNews)
        mapiReg.isDefaultNewsClient = true;
      else
        mapiReg.isDefaultMailClient = true;
    }
  } else {
    promptMessage = mapiBundle.getFormattedString("alreadyDefault" + str,
                                                  [brandShortName]);

    prompt.alert(window, promptTitle, promptMessage);
  }
}
#endif

function setColorWell(menu) 
{
  // Find the colorWell and colorPicker in the hierarchy.
  var colorWell = menu.firstChild.nextSibling;
  var colorPicker = menu.firstChild.nextSibling.nextSibling.firstChild;

  // Extract color from colorPicker and assign to colorWell.
  var color = colorPicker.getAttribute('color');
  colorWell.style.backgroundColor = color;
}

function StartPageCheck()
{
  var checked = document.getElementById("mailnewsStartPageEnabled").checked;
  document.getElementById("mailnewsStartPageUrl").disabled = !checked;
}

function setHomePageToDefaultPage(folderFieldId)
{
  var homePageField = document.getElementById(folderFieldId);
  var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                              .getService(Components.interfaces.nsIPrefService);
  var prefs = prefService.getDefaultBranch(null);
  var url = prefs.getComplexValue("mailnews.start_page.url",
                                  Components.interfaces.nsIPrefLocalizedString).data;
  homePageField.value = url;
}

function PlaySoundCheck()
{
  var playSound = document.getElementById("newMailNotification").checked;
  var playSoundType = document.getElementById("newMailNotificationType");
  playSoundType.disabled = !playSound;

  var disableCustomUI = !(playSound && playSoundType.value == 1);
  var mailnewsSoundFileUrl = document.getElementById("mailnewsSoundFileUrl");

  mailnewsSoundFileUrl.disabled = disableCustomUI
  document.getElementById("preview").disabled = disableCustomUI || (mailnewsSoundFileUrl.value == "");
  document.getElementById("browse").disabled = disableCustomUI;
}

function onCustomWavInput()
{
  document.getElementById("preview").disabled = (document.getElementById("mailnewsSoundFileUrl").value == "");
}

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function Browse()
{
  var fp = Components.classes["@mozilla.org/filepicker;1"]
                       .createInstance(nsIFilePicker);

  // XXX todo, persist the last sound directory and pass it in
  // XXX todo filter by .wav
  fp.init(window, document.getElementById("browse").getAttribute("filepickertitle"), nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterAll);

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) {
    var mailnewsSoundFileUrl = document.getElementById("mailnewsSoundFileUrl");

    // convert the nsILocalFile into a nsIFile url 
    mailnewsSoundFileUrl.value = fp.fileURL.spec;
  }

  onCustomWavInput();
}

var gSound = null;

function PreviewSound()
{
  var soundURL = document.getElementById("mailnewsSoundFileUrl").value;

  if (!gSound)
    gSound = Components.classes["@mozilla.org/sound;1"].createInstance(Components.interfaces.nsISound);

  if (soundURL.indexOf("file://") == -1) {
    // XXX todo see if we can create a nsIURL from the native file path
    // otherwise, play a system sound
    gSound.playSystemSound(soundURL);
  }
  else {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
    var url = ioService.newURI(soundURL, null, null);
    gSound.play(url)
  }
}
