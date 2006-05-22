# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Thunderbird Preferences System.
#
# The Initial Developer of the Original Code is
# Scott MacGregor.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Scott MacGregor <mscott@mozilla.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

var gNotificationsDialog = {
  mSound: null,
  SoundUrlLocation: null,
  newMailNotificationType: null,

  init: function()
  {
    this.soundUrlLocation = document.getElementById("soundUrlLocation");
    this.newMailNotificationType = document.getElementById("newMailNotificationType");
    this.systemSoundCheck();
  },

  convertURLToLocalFile: function(aFileURL)
  {
    // convert the file url into a nsILocalFile
    if (aFileURL)
    {
      var ios = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
      var fph = ios.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
      return fph.getFileFromURLSpec(aFileURL);
    } 
    else
      return null;
  },

  readSoundLocation: function()
  {
    var soundUrlLocation = document.getElementById("soundUrlLocation");
    soundUrlLocation.value = document.getElementById("mail.biff.play_sound.url").value;     
    soundUrlLocation.label = this.convertURLToLocalFile(soundUrlLocation.value).leafName;
    soundUrlLocation.image = "moz-icon://" + soundUrlLocation.label + "?size=16";
    return undefined;
  },

  previewSound: function ()
  {  
    if (!this.mSound)
      this.mSound = Components.classes["@mozilla.org/sound;1"].createInstance(Components.interfaces.nsISound);
    
    var soundLocation;
    soundLocation = this.newMailNotificationType.value == 1 ? 
                    this.soundUrlLocation.value : "_moz_mailbeep"

    if (soundLocation.indexOf("file://") == -1) 
      this.mSound.playSystemSound(soundLocation);
    else 
    {
      var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
      this.mSound.play(ioService.newURI(soundLocation, null, null));
    }
  }, 

  browseForSoundFile: function ()
  {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);

    // if we already have a sound file, then use the path for that sound file
    // as the initial path in the dialog.
    var localFile = this.convertURLToLocalFile(this.soundUrlLocation.value);
    if (localFile)
      fp.displayDirectory = localFile;

    // XXX todo, persist the last sound directory and pass it in
    fp.init(window, document.getElementById("bundlePreferences").getString("soundFilePickerTitle"), nsIFilePicker.modeOpen);
    fp.appendFilter("*.wav", "*.wav");

    var ret = fp.show();
    if (ret == nsIFilePicker.returnOK) 
    {
      // convert the nsILocalFile into a nsIFile url 
      // this.soundUrlLocation.value = fp.fileURL.spec;
      document.getElementById("mail.biff.play_sound.url").value = fp.fileURL.spec;
      this.readSoundLocation(); // XXX We shouldn't have to be doing this by hand
    }
  }, 

  systemSoundCheck: function ()
  {
    this.soundUrlLocation.disabled = this.newMailNotificationType.value != 1 ? true : false;
  },
};

