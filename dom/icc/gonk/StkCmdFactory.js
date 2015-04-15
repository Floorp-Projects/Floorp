/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let RIL = {};
Cu.import("resource://gre/modules/ril_consts.js", RIL);

const GONK_STKCMDFACTORY_CONTRACTID = "@mozilla.org/icc/stkcmdfactory;1";
const GONK_STKCMDFACTORY_CID = Components.ID("{7a663440-e336-11e4-8fd5-c3140a7ff307}");

/**
 * Helper Utilities to convert JS Objects to IDL Objects.
 */

/**
 * To map { timeUnit, timeInterval } into StkDuration.
 */
function mapDurationToStkDuration(aDuration) {
  return (aDuration)
    ? new StkDuration(aDuration.timeUnit, aDuration.timeInterval)
    : null;
}

/**
 * To map { iconSelfExplanatory, icons } into StkIconInfo.
 */
function mapIconInfoToStkIconInfo(aIconInfo) {
  let mapIconToStkIcon = function(aIcon) {
    return new StkIcon(aIcon.width, aIcon.height,
                       aIcon.codingScheme, aIcon.pixels);
  };

  return (aIconInfo &&
          aIconInfo.icons !== undefined &&
          aIconInfo.iconSelfExplanatory !== undefined)
    ? new StkIconInfo(aIconInfo.iconSelfExplanatory,
                      aIconInfo.icons.map(mapIconToStkIcon))
    : null;
}

/**
 * Helper Utilities to append the STK attributes to System Message.
 */

function appendDuration(aTarget, aStkDuration) {
  aTarget.timeUnit = aStkDuration.timeUnit;
  aTarget.timeInterval = aStkDuration.timeInterval;
}

function appendIconInfo(aTarget, aStkIconInfo) {
  aTarget.iconSelfExplanatory = aStkIconInfo.iconSelfExplanatory;
  aTarget.icons = aStkIconInfo.getIcons().map(function(aStkIcon) {
    return {
      width: aStkIcon.width,
      height: aStkIcon.height,
      codingScheme: RIL.ICC_IMG_CODING_SCHEME_TO_GECKO[aStkIcon.codingScheme],
      pixels: aStkIcon.getPixels()
    };
  });
}

/**
 * The implementation of the data types used in variant types of
 * StkProactiveCommand, StkTerminalResponse, StkDownloadEvent.
 */

function StkDuration(aTimeUnit, aTimeInterval) {
  this.timeUnit = aTimeUnit;
  this.timeInterval = aTimeInterval;
}
StkDuration.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStkDuration]),

  // nsIStkDuration
  timeUnit: 0,
  timeInterval: 0
};

function StkIcon(aWidth, aHeight, aCodingScheme, aPixels) {
  this.width = aWidth;
  this.height = aHeight;
  this.codingScheme = this.IMG_CODING_SCHEME[aCodingScheme];
  this.pixels = aPixels.slice();
}
StkIcon.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStkIcon]),

  // Cache pixels for getPixels()
  pixels: null,

  // Scheme mapping.
  IMG_CODING_SCHEME: {
    "basic": Ci.nsIStkIcon.CODING_SCHEME_BASIC,
    "color": Ci.nsIStkIcon.CODING_SCHEME_COLOR,
    "color-transparency": Ci.nsIStkIcon.CODING_SCHEME_COLOR_TRANSPARENCY
  },

  // StkIcon
  width: 0,
  height: 0,
  codingScheme: 0,
  getPixels: function(aCount) {
    if (!this.pixels) {
      if (aCount) {
        aCount.value = 0;
      }
      return null;
    }

    if (aCount) {
      aCount.value = this.pixels.length;
    }

    return this.pixels.slice();
  }
};

function StkIconInfo(aIconSelfExplanatory, aStkIcons) {
  this.iconSelfExplanatory = aIconSelfExplanatory;
  this.icons = aStkIcons;
}
StkIconInfo.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStkIconInfo]),

  // Cache the list of StkIcon(s) for getIcons()
  icons: null,

  // nsIStkIconInfo
  iconSelfExplanatory: false,

  getIcons: function(aCount) {
    if (!this.icons) {
      if (aCount) {
        aCount.value = 0;
      }
      return null;
    }

    if (aCount) {
      aCount.value = this.icons.length;
    }

    return this.icons.slice();
  }
};

function StkItem(aIdentifier, aText, aStkIconInfo) {
  this.identifier = aIdentifier;
  if (aText !== undefined) {
    this.text = aText;
  }
  this.iconInfo = aStkIconInfo;
}
StkItem.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStkItem]),

  // nsIStkItem
  identifier: 0,
  text: null,
  iconInfo: null
};

function StkTimer(aTimerId, aTimerValue, aTimerAction) {
  this.timerId = aTimerId;
  if (aTimerValue !== undefined &&
      aTimerValue !== null) {
    this.timerValue = aTimerValue;
  }
  this.timerAction = aTimerAction;
}
StkTimer.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStkTimer]),

  // nsIStkTimer
  timerId: 0,
  timerValue: Ci.nsIStkTimer.TIMER_VALUE_INVALID,
  timerAction: 0
};

/**
 * The implementation of nsIStkProactiveCommand set and paired JS object set.
 */
function StkProactiveCommand(aCommandDetails) {
  this.commandNumber = aCommandDetails.commandNumber;
  this.typeOfCommand = aCommandDetails.typeOfCommand;
  this.commandQualifier = aCommandDetails.commandQualifier;
}
StkProactiveCommand.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd]),

  // nsIStkProactiveCmd
  commandNumber: 0,
  typeOfCommand: 0,
  commandQualifier: 0
};

function StkCommandMessage(aStkProactiveCmd) {
  this.commandNumber = aStkProactiveCmd.commandNumber;
  this.typeOfCommand = aStkProactiveCmd.typeOfCommand;
  this.commandQualifier = aStkProactiveCmd.commandQualifier;
}
StkCommandMessage.prototype = {
  commandNumber: 0,
  typeOfCommand: 0,
  commandQualifier: 0,
  options: null
};

function StkPollIntervalCmd(aCommandDetails) {
  // Call |StkProactiveCommand| constructor.
  StkProactiveCommand.call(this, aCommandDetails);

  this.duration = mapDurationToStkDuration(aCommandDetails.options);
}
StkPollIntervalCmd.prototype = Object.create(StkProactiveCommand.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkPollIntervalCmd])
  },

  // nsIStkPollIntervalCmd
  duration: { value: null, writable: true }
});

function StkPollIntervalMessage(aStkPollIntervalCmd) {
  // Call |StkCommandMessage| constructor.
  StkCommandMessage.call(this, aStkPollIntervalCmd);

  this.options = {};
  appendDuration(this.options, aStkPollIntervalCmd.duration);
}
StkPollIntervalMessage.prototype = Object.create(StkCommandMessage.prototype);

function StkProvideLocalInfoCmd(aCommandDetails) {
  // Call |StkProactiveCommand| constructor.
  StkProactiveCommand.call(this, aCommandDetails);

  this.localInfoType = aCommandDetails.options.localInfoType;
}
StkProvideLocalInfoCmd.prototype = Object.create(StkProactiveCommand.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkProvideLocalInfoCmd])
  },

  // nsIStkPollIntervalCmd
  localInfoType: { value: 0x00, writable: true }
});

function StkProvideLocalInfoMessage(aStkProvideLocalInfoCmd) {
  // Call |StkCommandMessage| constructor.
  StkCommandMessage.call(this, aStkProvideLocalInfoCmd);

  this.options = {
    localInfoType: aStkProvideLocalInfoCmd.localInfoType
  };
}
StkProvideLocalInfoMessage.prototype = Object.create(StkCommandMessage.prototype);

function StkSetupEventListCmd(aCommandDetails) {
  // Call |StkProactiveCommand| constructor.
  StkProactiveCommand.call(this, aCommandDetails);
  let eventList = aCommandDetails.options.eventList;
  if (eventList) {
    this.eventList = eventList.slice();
  }
}
StkSetupEventListCmd.prototype = Object.create(StkProactiveCommand.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkSetupEventListCmd])
  },

  // Cache eventList for getEventList()
  eventList: { value: null, writable: true },

  // nsIStkSetupEventListCmd
  getEventList: {
    value: function(aCount) {
      if (!this.eventList) {
        if (aCount) {
          aCount.value = 0;
        }
        return null;
      }

      if (aCount) {
        aCount.value = this.eventList.length;
      }

      return this.eventList.slice();
    }
  }
});

function StkSetupEventListMessage(aStkSetupEventListCmd) {
  // Call |StkCommandMessage| constructor.
  StkCommandMessage.call(this, aStkSetupEventListCmd);

  this.options = {
    eventList: null
  };

  let eventList = aStkSetupEventListCmd.getEventList();

  if (eventList && eventList.length > 0) {
    this.options.eventList = eventList;
  }
}
StkSetupEventListMessage.prototype = Object.create(StkCommandMessage.prototype);

function StkSetUpMenuCmd(aCommandDetails) {
  // Call |StkProactiveCommand| constructor.
  StkProactiveCommand.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  if (options.title !== undefined) {
    this.title = options.title;
  }

  this.items = options.items.map(function(aItem) {
    // For |SET-UP MENU|, the 1st item in |aItems| could be null as an
    // indication to the ME to remove the existing menu from the menu
    // system in the ME.
    return (aItem) ? new StkItem(aItem.identifier,
                                 aItem.text,
                                 mapIconInfoToStkIconInfo(aItem))
                   : null;
  });

  if (options.nextActionList) {
    this.nextActionList = options.nextActionList.slice();
  }

  this.iconInfo = mapIconInfoToStkIconInfo(options);

  this.isHelpAvailable = !!(options.isHelpAvailable);
}
StkSetUpMenuCmd.prototype = Object.create(StkProactiveCommand.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkSetUpMenuCmd])
  },

  // Cache items for getItems()
  items: { value: null, writable: true },

  // Cache items for getNextActionList()
  nextActionList: { value: null, writable: true },

  // nsIStkSetUpMenuCmd
  title: { value: null, writable: true },

  getItems: {
    value: function(aCount) {
      if (!this.items) {
        if (aCount) {
          aCount.value = 0;
        }
        return null;
      }

      if (aCount) {
        aCount.value = this.items.length;
      }

      return this.items.slice();
    }
  },

  getNextActionList: {
    value: function(aCount) {
      if (!this.nextActionList) {
        if (aCount) {
          aCount.value = 0;
        }
        return null;
      }

      if (aCount) {
        aCount.value = this.nextActionList.length;
      }

      return this.nextActionList.slice();
    }
  },

  iconInfo: { value: null, writable: true },
  isHelpAvailable: { value: false, writable: true }
});

function StkSetUpMenuMessage(aStkSetUpMenuCmd) {
  // Call |StkCommandMessage| constructor.
  StkCommandMessage.call(this, aStkSetUpMenuCmd);

  this.options = {
    items: aStkSetUpMenuCmd.getItems().map(function(aStkItem) {
      if (!aStkItem) {
        return null;
      }

      let item = {
        identifier: aStkItem.identifier,
        text: aStkItem.text
      };

      if (aStkItem.iconInfo) {
        appendIconInfo(item, aStkItem.iconInfo);
      }

      return item;
    }),
    isHelpAvailable: aStkSetUpMenuCmd.isHelpAvailable,
    title: aStkSetUpMenuCmd.title
  };

  let nextActionList = aStkSetUpMenuCmd.getNextActionList();
  if (nextActionList && nextActionList.length > 0) {
    this.options.nextActionList = nextActionList;
  }

  if (aStkSetUpMenuCmd.iconInfo) {
    appendIconInfo(this.options, aStkSetUpMenuCmd.iconInfo);
  }
}
StkSetUpMenuMessage.prototype = Object.create(StkCommandMessage.prototype);

function StkSelectItemCmd(aCommandDetails) {
  // Call |StkSetUpMenuCmd| constructor.
  StkSetUpMenuCmd.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  this.presentationType = options.presentationType;

  if (options.defaultItem !== undefined &&
      options.defaultItem !== null) {
    this.defaultItem = options.defaultItem;
  }
}
StkSelectItemCmd.prototype = Object.create(StkSetUpMenuCmd.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkSetUpMenuCmd,
                                  Ci.nsIStkSelectItemCmd])
  },

  // nsIStkSelectItemCmd
  presentationType: {
    value: 0,
    writable: true
  },

  defaultItem: {
    value: Ci.nsIStkSelectItemCmd.DEFAULT_ITEM_INVALID,
    writable: true
  }
});

function StkSelectItemMessage(aStkSelectItemCmd) {
  // Call |StkSetUpMenuMessage| constructor.
  StkSetUpMenuMessage.call(this, aStkSelectItemCmd);

  this.options.presentationType = aStkSelectItemCmd.presentationType;

  if (aStkSelectItemCmd.defaultItem !== Ci.nsIStkSelectItemCmd.DEFAULT_ITEM_INVALID) {
    this.options.defaultItem = aStkSelectItemCmd.defaultItem;
  }
}
StkSelectItemMessage.prototype = Object.create(StkSetUpMenuMessage.prototype);

function StkTextMessageCmd(aCommandDetails) {
  // Call |StkProactiveCommand| constructor.
  StkProactiveCommand.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  if (options.text !== undefined) {
    this.text = options.text;
  }

  this.iconInfo = mapIconInfoToStkIconInfo(options);
}
StkTextMessageCmd.prototype = Object.create(StkProactiveCommand.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkTextMessageCmd])
  },

  // nsIStkTextMessageCmd
  text: { value: null, writable: true },
  iconInfo: { value: null, writable: true }
});

function StkTextMessage(aStkTextMessageCmd) {
  // Call |StkCommandMessage| constructor.
  StkCommandMessage.call(this, aStkTextMessageCmd);

  this.options = {
    text: aStkTextMessageCmd.text
  };

  if (aStkTextMessageCmd.iconInfo) {
    appendIconInfo(this.options, aStkTextMessageCmd.iconInfo);
  }
}
StkTextMessage.prototype = Object.create(StkCommandMessage.prototype);

function StkDisplayTextCmd(aCommandDetails) {
  // Call |StkTextMessageCmd| constructor.
  StkTextMessageCmd.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  this.duration = mapDurationToStkDuration(options.duration);

  this.isHighPriority = !!(options.isHighPriority);
  this.userClear = !!(options.userClear);
  this.responseNeeded = !!(options.responseNeeded);
}
StkDisplayTextCmd.prototype = Object.create(StkTextMessageCmd.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkTextMessageCmd,
                                  Ci.nsIStkDisplayTextCmd])
  },

  // nsIStkDisplayTextCmd
  duration: { value: null, writable: true },
  isHighPriority: { value: false, writable: true },
  userClear: { value: false, writable: true },
  responseNeeded: { value: false, writable: true }
});

function StkDisplayTextMessage(aStkDisplayTextCmd) {
  // Call |StkTextMessage| constructor.
  StkTextMessage.call(this, aStkDisplayTextCmd);

  this.options.isHighPriority = aStkDisplayTextCmd.isHighPriority;
  this.options.userClear = aStkDisplayTextCmd.userClear;
  this.options.responseNeeded = aStkDisplayTextCmd.responseNeeded;

  if (aStkDisplayTextCmd.duration) {
    this.options.duration = {};
    appendDuration(this.options.duration, aStkDisplayTextCmd.duration);
  }
}
StkDisplayTextMessage.prototype = Object.create(StkTextMessage.prototype);

function StkInputCmd(aCommandDetails) {
  // Call |StkProactiveCommand| constructor.
  StkProactiveCommand.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  if (options.text !== undefined) {
    this.text = options.text;
  }

  this.duration = mapDurationToStkDuration(options.duration);

  if (options.defaultText !== undefined) {
    this.defaultText = options.defaultText;
  }

  this.isAlphabet = !!(options.isAlphabet);
  this.isUCS2 = !!(options.isUCS2);
  this.isHelpAvailable = !!(options.isHelpAvailable);

  this.iconInfo = mapIconInfoToStkIconInfo(options);
}
StkInputCmd.prototype = Object.create(StkProactiveCommand.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkInputCmd])
  },

  // nsIStkInputCmd
  text: { value: null, writable: true },
  duration: { value: null, writable: true },
  minLength: { value: 1, writable: true },
  maxLength: { value: 1, writable: true },
  defaultText: { value: null, writable: true },
  isAlphabet: { value: false, writable: true },
  isUCS2: { value: false, writable: true },
  isHelpAvailable: { value: false, writable: true },
  iconInfo: { value: null, writable: true }
});

function StkInputMessage(aStkInputCmd) {
  // Call |StkCommandMessage| constructor.
  StkCommandMessage.call(this, aStkInputCmd);

  this.options = {
    text: aStkInputCmd.text,
    minLength: aStkInputCmd.minLength,
    maxLength: aStkInputCmd.maxLength,
    isAlphabet: aStkInputCmd.isAlphabet,
    isUCS2: aStkInputCmd.isUCS2,
    isHelpAvailable: aStkInputCmd.isHelpAvailable,
    defaultText: aStkInputCmd.defaultText
  };

  if (aStkInputCmd.duration) {
    this.options.duration = {};
    appendDuration(this.options.duration, aStkInputCmd.duration);
  }

  if (aStkInputCmd.iconInfo) {
    appendIconInfo(this.options, aStkInputCmd.iconInfo);
  }
}
StkInputMessage.prototype = Object.create(StkCommandMessage.prototype);

function StkInputKeyCmd(aCommandDetails) {
  // Call |StkInputCmd| constructor.
  StkInputCmd.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  // Note: For STK_CMD_INKEY,
  // this.minLength = this.maxLength = 1;

  this.isYesNoRequested = !!(options.isYesNoRequested);
}
StkInputKeyCmd.prototype = Object.create(StkInputCmd.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkInputCmd,
                                  Ci.nsIStkInputKeyCmd])
  },

  // nsIStkInputKeyCmd
  isYesNoRequested: { value: false, writable: true }
});

function StkInputKeyMessage(aStkInputKeyCmd) {
  // Call |StkInputMessage| constructor.
  StkInputMessage.call(this, aStkInputKeyCmd);

  this.options.isYesNoRequested = aStkInputKeyCmd.isYesNoRequested;
}
StkInputKeyMessage.prototype = Object.create(StkInputMessage.prototype);

function StkInputTextCmd(aCommandDetails) {
  // Call |StkInputCmd| constructor.
  StkInputCmd.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  this.minLength = options.minLength;
  this.maxLength = options.maxLength;

  this.hideInput = !!(options.hideInput);
  this.isPacked = !!(options.isPacked);
}
StkInputTextCmd.prototype = Object.create(StkInputCmd.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkInputCmd,
                                  Ci.nsIStkInputTextCmd])
  },

  // nsIStkInputTextCmd
  hideInput: { value: false, writable: true },
  isPacked: { value: false, writable: true }
});

function StkInputTextMessage(aStkInputTextCmd) {
  // Call |StkInputMessage| constructor.
  StkInputMessage.call(this, aStkInputTextCmd);

  this.options.hideInput = aStkInputTextCmd.hideInput;
  this.options.isPacked = aStkInputTextCmd.isPacked;
}
StkInputTextMessage.prototype = Object.create(StkInputMessage.prototype);

function StkSetUpCallCmd(aCommandDetails) {
  // Call |StkProactiveCommand| constructor.
  StkProactiveCommand.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  let confirmMessage = options.confirmMessage;
  let callMessage = options.callMessage;

  this.address = options.address;

  if(confirmMessage) {
    if (confirmMessage.text !== undefined) {
      this.confirmText = confirmMessage.text;
    }
    this.confirmIconInfo = mapIconInfoToStkIconInfo(confirmMessage);
  }

  if(callMessage) {
    if (callMessage.text !== undefined) {
      this.callText = callMessage.text;
    }
    this.callIconInfo = mapIconInfoToStkIconInfo(callMessage);
  }

  this.duration = mapDurationToStkDuration(options.duration);
}
StkSetUpCallCmd.prototype = Object.create(StkProactiveCommand.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkSetUpCallCmd])
  },

  // nsIStkSetUpCallCmd
  address: { value: null, writable: true },
  confirmText: { value: null, writable: true },
  confirmIconInfo: { value: null, writable: true },
  callText: { value: null, writable: true },
  callIconInfo: { value: null, writable: true },
  duration: { value: null, writable: true }
});

function StkSetUpCallMessage(aStkSetUpCallCmd) {
  // Call |StkCommandMessage| constructor.
  StkCommandMessage.call(this, aStkSetUpCallCmd);

  this.options = {
    address: aStkSetUpCallCmd.address
  };

  if (aStkSetUpCallCmd.confirmText !== null ||
      aStkSetUpCallCmd.confirmIconInfo) {
    let confirmMessage = {
      text: aStkSetUpCallCmd.confirmText
    };
    if (aStkSetUpCallCmd.confirmIconInfo) {
      appendIconInfo(confirmMessage, aStkSetUpCallCmd.confirmIconInfo);
    }
    this.options.confirmMessage = confirmMessage;
  }

  if (aStkSetUpCallCmd.callText !== null ||
      aStkSetUpCallCmd.callIconInfo) {
    let callMessage = {
      text: aStkSetUpCallCmd.callText
    };
    if (aStkSetUpCallCmd.callIconInfo) {
      appendIconInfo(callMessage, aStkSetUpCallCmd.callIconInfo);
    }
    this.options.callMessage = callMessage;
  }

  if (aStkSetUpCallCmd.duration) {
    this.options.duration = {};
    appendDuration(this.options.duration, aStkSetUpCallCmd.duration);
  }
}
StkSetUpCallMessage.prototype = Object.create(StkCommandMessage.prototype);

function StkBrowserSettingCmd(aCommandDetails) {
  // Call |StkProactiveCommand| constructor.
  StkProactiveCommand.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  this.url = options.url;

  this.mode = options.mode;

  let confirmMessage = options.confirmMessage;

  if(confirmMessage) {
    if (confirmMessage.text !== undefined) {
      this.confirmText = confirmMessage.text;
    }
    this.confirmIconInfo = mapIconInfoToStkIconInfo(confirmMessage);
  }
}
StkBrowserSettingCmd.prototype = Object.create(StkProactiveCommand.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkBrowserSettingCmd])
  },

  // nsIStkBrowserSettingCmd
  url: { value: null, writable: true },
  mode: { value: 0, writable: true },
  confirmText: { value: null, writable: true },
  confirmIconInfo: { value: null, writable: true }
});

function StkBrowserSettingMessage(aStkBrowserSettingCmd) {
  // Call |StkCommandMessage| constructor.
  StkCommandMessage.call(this, aStkBrowserSettingCmd);

  this.options = {
    url: aStkBrowserSettingCmd.url,
    mode: aStkBrowserSettingCmd.mode
  };

  if (aStkBrowserSettingCmd.confirmText !== null ||
      aStkBrowserSettingCmd.confirmIconInfo) {
    let confirmMessage = {
      text: aStkBrowserSettingCmd.confirmText
    };
    if (aStkBrowserSettingCmd.confirmIconInfo) {
      appendIconInfo(confirmMessage, aStkBrowserSettingCmd.confirmIconInfo);
    }
    this.options.confirmMessage = confirmMessage;
  }
}
StkBrowserSettingMessage.prototype = Object.create(StkCommandMessage.prototype);

function StkPlayToneCmd(aCommandDetails) {
  // Call |StkProactiveCommand| constructor.
  StkProactiveCommand.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  if(options.text !== undefined) {
    this.text = options.text;
  }

  if (options.tone !== undefined &&
      options.tone !== null) {
    this.tone = options.tone;
  }

  if (options.isVibrate) {
    this.isVibrate = true;
  }

  this.duration = mapDurationToStkDuration(options.duration);

  this.iconInfo = mapIconInfoToStkIconInfo(options);
}
StkPlayToneCmd.prototype = Object.create(StkProactiveCommand.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkPlayToneCmd])
  },

  // nsIStkPlayToneCmd
  text: { value: null, writable: true },
  tone: { value: Ci.nsIStkPlayToneCmd.TONE_TYPE_INVALID, writable: true },
  duration: { value: null, writable: true },
  isVibrate: { value: false, writable: true },
  iconInfo: { value: null, writable: true }
});

function StkPlayToneMessage(aStkPlayToneCmd) {
  // Call |StkCommandMessage| constructor.
  StkCommandMessage.call(this, aStkPlayToneCmd);

  this.options = {
    isVibrate: aStkPlayToneCmd.isVibrate,
    text: aStkPlayToneCmd.text
  };

  if (aStkPlayToneCmd.tone != Ci.nsIStkPlayToneCmd.TONE_TYPE_INVALID) {
    this.options.tone = aStkPlayToneCmd.tone;
  }

  if (aStkPlayToneCmd.duration) {
    this.options.duration = {};
    appendDuration(this.options.duration, aStkPlayToneCmd.duration);
  }

  if (aStkPlayToneCmd.iconInfo) {
    appendIconInfo(this.options, aStkPlayToneCmd.iconInfo);
  }
}
StkPlayToneMessage.prototype = Object.create(StkCommandMessage.prototype);

function StkTimerManagementCmd(aCommandDetails) {
  // Call |StkProactiveCommand| constructor.
  StkProactiveCommand.call(this, aCommandDetails);

  let options = aCommandDetails.options;

  this.timerInfo = new StkTimer(options.timerId,
                                options.timerValue,
                                options.timerAction);

}
StkTimerManagementCmd.prototype = Object.create(StkProactiveCommand.prototype, {
  QueryInterface: {
    value: XPCOMUtils.generateQI([Ci.nsIStkProactiveCmd,
                                  Ci.nsIStkTimerManagementCmd])
  },

  // nsIStkTimerManagementCmd
  timerInfo: { value: null, writable: true }
});

function StkTimerMessage(aStkTimerManagementCmd) {
  // Call |StkCommandMessage| constructor.
  StkCommandMessage.call(this, aStkTimerManagementCmd);

  let timerInfo = aStkTimerManagementCmd.timerInfo;

  this.options = {
    timerId: timerInfo.timerId,
    timerAction: timerInfo.timerAction
  };

  if (timerInfo.timerValue !== Ci.nsIStkTimer.TIMER_VALUE_INVALID) {
    this.options.timerValue = timerInfo.timerValue;
  }
}
StkTimerMessage.prototype = Object.create(StkCommandMessage.prototype);

/**
 * Command Prototype Mappings.
 */
let CmdPrototypes = {};
CmdPrototypes[RIL.STK_CMD_REFRESH] = StkProactiveCommand;
CmdPrototypes[RIL.STK_CMD_POLL_INTERVAL] = StkPollIntervalCmd;
CmdPrototypes[RIL.STK_CMD_POLL_OFF] = StkProactiveCommand;
CmdPrototypes[RIL.STK_CMD_PROVIDE_LOCAL_INFO] = StkProvideLocalInfoCmd;
CmdPrototypes[RIL.STK_CMD_SET_UP_EVENT_LIST] = StkSetupEventListCmd;
CmdPrototypes[RIL.STK_CMD_SET_UP_MENU] = StkSetUpMenuCmd;
CmdPrototypes[RIL.STK_CMD_SELECT_ITEM] = StkSelectItemCmd;
CmdPrototypes[RIL.STK_CMD_DISPLAY_TEXT] = StkDisplayTextCmd;
CmdPrototypes[RIL.STK_CMD_SET_UP_IDLE_MODE_TEXT] = StkTextMessageCmd;
CmdPrototypes[RIL.STK_CMD_SEND_SS] = StkTextMessageCmd;
CmdPrototypes[RIL.STK_CMD_SEND_USSD] = StkTextMessageCmd;
CmdPrototypes[RIL.STK_CMD_SEND_SMS] = StkTextMessageCmd;
CmdPrototypes[RIL.STK_CMD_SEND_DTMF] = StkTextMessageCmd;
CmdPrototypes[RIL.STK_CMD_GET_INKEY] = StkInputKeyCmd;
CmdPrototypes[RIL.STK_CMD_GET_INPUT] = StkInputTextCmd;
CmdPrototypes[RIL.STK_CMD_SET_UP_CALL] = StkSetUpCallCmd;
CmdPrototypes[RIL.STK_CMD_LAUNCH_BROWSER] = StkBrowserSettingCmd;
CmdPrototypes[RIL.STK_CMD_PLAY_TONE] = StkPlayToneCmd;
CmdPrototypes[RIL.STK_CMD_TIMER_MANAGEMENT] = StkTimerManagementCmd;
CmdPrototypes[RIL.STK_CMD_OPEN_CHANNEL] = StkTextMessageCmd;
CmdPrototypes[RIL.STK_CMD_CLOSE_CHANNEL] = StkTextMessageCmd;
CmdPrototypes[RIL.STK_CMD_SEND_DATA] = StkTextMessageCmd;
CmdPrototypes[RIL.STK_CMD_RECEIVE_DATA] = StkTextMessageCmd;

/**
 * Message Prototype Mappings.
 */
let MsgPrototypes = {};
MsgPrototypes[RIL.STK_CMD_REFRESH] = StkCommandMessage;
MsgPrototypes[RIL.STK_CMD_POLL_INTERVAL] = StkPollIntervalMessage;
MsgPrototypes[RIL.STK_CMD_POLL_OFF] = StkCommandMessage;
MsgPrototypes[RIL.STK_CMD_PROVIDE_LOCAL_INFO] = StkProvideLocalInfoMessage;
MsgPrototypes[RIL.STK_CMD_SET_UP_EVENT_LIST] = StkSetupEventListMessage;
MsgPrototypes[RIL.STK_CMD_SET_UP_MENU] = StkSetUpMenuMessage;
MsgPrototypes[RIL.STK_CMD_SELECT_ITEM] = StkSelectItemMessage;
MsgPrototypes[RIL.STK_CMD_DISPLAY_TEXT] = StkDisplayTextMessage;
MsgPrototypes[RIL.STK_CMD_SET_UP_IDLE_MODE_TEXT] = StkTextMessage;
MsgPrototypes[RIL.STK_CMD_SEND_SS] = StkTextMessage;
MsgPrototypes[RIL.STK_CMD_SEND_USSD] = StkTextMessage;
MsgPrototypes[RIL.STK_CMD_SEND_SMS] = StkTextMessage;
MsgPrototypes[RIL.STK_CMD_SEND_DTMF] = StkTextMessage;
MsgPrototypes[RIL.STK_CMD_GET_INKEY] = StkInputKeyMessage;
MsgPrototypes[RIL.STK_CMD_GET_INPUT] = StkInputTextMessage;
MsgPrototypes[RIL.STK_CMD_SET_UP_CALL] = StkSetUpCallMessage;
MsgPrototypes[RIL.STK_CMD_LAUNCH_BROWSER] = StkBrowserSettingMessage;
MsgPrototypes[RIL.STK_CMD_PLAY_TONE] = StkPlayToneMessage;
MsgPrototypes[RIL.STK_CMD_TIMER_MANAGEMENT] = StkTimerMessage;
MsgPrototypes[RIL.STK_CMD_OPEN_CHANNEL] = StkTextMessage;
MsgPrototypes[RIL.STK_CMD_CLOSE_CHANNEL] = StkTextMessage;
MsgPrototypes[RIL.STK_CMD_SEND_DATA] = StkTextMessage;
MsgPrototypes[RIL.STK_CMD_RECEIVE_DATA] = StkTextMessage;

/**
 * QueryInterface Mappings.
 */
let QueriedIFs = {};
QueriedIFs[RIL.STK_CMD_REFRESH] = Ci.nsIStkProactiveCmd;
QueriedIFs[RIL.STK_CMD_POLL_INTERVAL] = Ci.nsIStkPollIntervalCmd;
QueriedIFs[RIL.STK_CMD_POLL_OFF] = Ci.nsIStkProactiveCmd;
QueriedIFs[RIL.STK_CMD_PROVIDE_LOCAL_INFO] = Ci.nsIStkProvideLocalInfoCmd;
QueriedIFs[RIL.STK_CMD_SET_UP_EVENT_LIST] = Ci.nsIStkSetupEventListCmd;
QueriedIFs[RIL.STK_CMD_SET_UP_MENU] = Ci.nsIStkSetUpMenuCmd;
QueriedIFs[RIL.STK_CMD_SELECT_ITEM] = Ci.nsIStkSelectItemCmd;
QueriedIFs[RIL.STK_CMD_DISPLAY_TEXT] = Ci.nsIStkDisplayTextCmd;
QueriedIFs[RIL.STK_CMD_SET_UP_IDLE_MODE_TEXT] = Ci.nsIStkTextMessageCmd;
QueriedIFs[RIL.STK_CMD_SEND_SS] = Ci.nsIStkTextMessageCmd;
QueriedIFs[RIL.STK_CMD_SEND_USSD] = Ci.nsIStkTextMessageCmd;
QueriedIFs[RIL.STK_CMD_SEND_SMS] = Ci.nsIStkTextMessageCmd;
QueriedIFs[RIL.STK_CMD_SEND_DTMF] = Ci.nsIStkTextMessageCmd;
QueriedIFs[RIL.STK_CMD_GET_INKEY] = Ci.nsIStkInputKeyCmd;
QueriedIFs[RIL.STK_CMD_GET_INPUT] = Ci.nsIStkInputTextCmd;
QueriedIFs[RIL.STK_CMD_SET_UP_CALL] = Ci.nsIStkSetUpCallCmd;
QueriedIFs[RIL.STK_CMD_LAUNCH_BROWSER] = Ci.nsIStkBrowserSettingCmd;
QueriedIFs[RIL.STK_CMD_PLAY_TONE] = Ci.nsIStkPlayToneCmd;
QueriedIFs[RIL.STK_CMD_TIMER_MANAGEMENT] = Ci.nsIStkTimerManagementCmd;
QueriedIFs[RIL.STK_CMD_OPEN_CHANNEL] = Ci.nsIStkTextMessageCmd;
QueriedIFs[RIL.STK_CMD_CLOSE_CHANNEL] = Ci.nsIStkTextMessageCmd;
QueriedIFs[RIL.STK_CMD_SEND_DATA] = Ci.nsIStkTextMessageCmd;
QueriedIFs[RIL.STK_CMD_RECEIVE_DATA] = Ci.nsIStkTextMessageCmd;

/**
 * StkProactiveCmdFactory
 */
function StkProactiveCmdFactory() {
}
StkProactiveCmdFactory.prototype = {
  classID: GONK_STKCMDFACTORY_CID,

  classInfo: XPCOMUtils.generateCI({classID: GONK_STKCMDFACTORY_CID,
                                    contractID: GONK_STKCMDFACTORY_CONTRACTID,
                                    classDescription: "StkProactiveCmdFactory",
                                    interfaces: [Ci.nsIStkCmdFactory],
                                    flags: Ci.nsIClassInfo.SINGLETON}),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIStkCmdFactory]),

  /**
   * nsIStkCmdFactory interface.
   */
  createCommand: function(aCommandDetails) {
    let cmdType = CmdPrototypes[aCommandDetails.typeOfCommand];

    if (typeof cmdType != "function") {
      throw new Error("Unknown Command Type: " + aCommandDetails.typeOfCommand);
    }

    return new cmdType(aCommandDetails);
  },

  createCommandMessage: function(aStkProactiveCmd) {
    let cmd = null;

    let msgType = MsgPrototypes[aStkProactiveCmd.typeOfCommand];

    if (typeof msgType != "function") {
      throw new Error("Unknown Command Type: " + aStkProactiveCmd.typeOfCommand);
    }

    // convert aStkProactiveCmd to it's concrete interface before creating
    // system message.
    try {
      cmd = aStkProactiveCmd.QueryInterface(QueriedIFs[aStkProactiveCmd.typeOfCommand]);
    } catch (e) {
      throw new Error("Failed to convert command into concrete class: " + e);
    }

    return new msgType(cmd);
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([StkProactiveCmdFactory]);
