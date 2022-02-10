import ConfigManager from "/core/helpers/config-manager.mjs";

browser.runtime.onInstalled.addListener(async (details) => {

  if (details.reason === "update" && details.previousVersion) {

    const Config = new ConfigManager("local");
    await Config.loaded;

    // migrate sync config from versions before 3.0.6 to local config
    if (isPreviousVersion(details.previousVersion, "3.0.6")) {
      const syncConfig = new ConfigManager("sync");
      await syncConfig.loaded;
      await Config.clear();
      Config.set(syncConfig.get());
    }

    // migrate config from version 2 to 3.0
    if (details.previousVersion[0] === "2") {
      // migrate trace settings
      const traceStyle = Config.get("Settings.Gesture.Trace.Style");
      if (traceStyle && traceStyle.strokeStyle && traceStyle.strokeStyle.length < 9) {
        traceStyle.strokeStyle = traceStyle.strokeStyle ? traceStyle.strokeStyle : "#00aaa0";
        if ("opacity" in traceStyle) {
          let aHex = Math.round(traceStyle.opacity * 255).toString(16);
          // add leading zero if string length is 1
          if (aHex.length === 1) aHex = "0" + aHex;
          traceStyle.strokeStyle += aHex;
        }
        else traceStyle.strokeStyle += "cc";
        Config.set("Settings.Gesture.Trace.Style", traceStyle);
      }

      // migrate command settings
      const commandStyle = Config.get("Settings.Gesture.Command.Style");
      if (commandStyle && commandStyle.backgroundColor && commandStyle.backgroundColor.length < 9) {
        commandStyle.backgroundColor = commandStyle.backgroundColor ? commandStyle.backgroundColor : "#000000";
        if ("backgroundOpacity" in commandStyle) {
          let aHex = Math.round(commandStyle.backgroundOpacity * 255).toString(16);
          // add leading zero if string length is 1
          if (aHex.length === 1) aHex = "0" + aHex;
          commandStyle.backgroundColor += aHex;
        }
        else commandStyle.backgroundColor += "80";

        if ("color" in commandStyle) {
          commandStyle.fontColor = commandStyle.color + "ff";
          delete commandStyle.color;
        }
        Config.set("Settings.Gesture.Command.Style", commandStyle);
      }

      // migrate gestures
      const gestures = Config.get("Gestures");
      if (gestures) {
        for (const gestureItem of gestures.reverse()) {
          convertOldGestureFormatToNewFormat(gestureItem)
        }
        Config.set("Gestures", gestures);
      }

      // migrate rocker and wheel gestures
      const rockerLeft = Config.get("Settings.Rocker.leftMouseClick");
      if (rockerLeft) {
        convertOldCommandFormatToNewFormat(rockerLeft);
        Config.set("Settings.Rocker.leftMouseClick", rockerLeft);
      }
      const rockerRight = Config.get("Settings.Rocker.rightMouseClick");
      if (rockerRight) {
        convertOldCommandFormatToNewFormat(rockerRight);
        Config.set("Settings.Rocker.rightMouseClick", rockerRight);
      }

      const wheelUp = Config.get("Settings.Wheel.wheelUp");
      if (wheelUp) {
        convertOldCommandFormatToNewFormat(wheelUp);
        Config.set("Settings.Wheel.wheelUp", wheelUp);
      }
      const wheelDown = Config.get("Settings.Wheel.wheelDown");
      if (wheelDown) {
        convertOldCommandFormatToNewFormat(wheelDown);
        Config.set("Settings.Wheel.wheelDown", wheelDown);
      }
    }

    // migrate blacklist entries to exclusions
    {
      const bl = Config.get("Blacklist");
      if (bl && bl.length > 0) {
        Config.set("Exclusions", bl);
        Config.remove("Blacklist");
      }
    }

    // migrate old default algorithm (strict)
    {
      const matchingAlgorithm = Config.get("Settings.Gesture.matchingAlgorithm");
      if (!matchingAlgorithm && isPreviousVersion(details.previousVersion, "3.0.8")) {
        Config.set("Settings.Gesture.matchingAlgorithm", "strict");
      }
    }

    // migrate theme
    {
      const theme = Config.get("Settings.General.theme");
      if (theme === "white" || theme === "default") {
        Config.set("Settings.General.theme", "light");
      }
    }

    // migrate search commands to new name
    if (isPreviousVersion(details.previousVersion, "3.2.0")) {
      const gestures = Config.get("Gestures");
      for (const gesture of gestures) {
        if (gesture.command.name === "SearchTextSelection") {
          gesture.command.name = "SearchTextSelectionInNewTab";
        }
        else if (gesture.command.name === "SearchClipboard") {
          gesture.command.name = "SearchClipboardInNewTab";
        }
      }
      Config.set("Gestures", gestures);
    }

    // add new scroll page up/down setting
    if (isPreviousVersion(details.previousVersion, "3.2.2")) {
      addSettingToCommand(Config, "ScrollPageUp", "scrollProportion", 95);
      addSettingToCommand(Config, "ScrollPageDown", "scrollProportion", 95);
    }
  }
});



function convertDirectionsToPattern (directions) {
  const pattern = [];
  for (const direction of directions) {
    switch (direction) {
      case "U": pattern.push([0, -1]); break;
      case "D": pattern.push([0, 1]); break;
      case "R": pattern.push([1, 0]); break;
      case "L": pattern.push([-1, 0]); break;
      default: pattern.push([0, 0]); break;
    }
  }
  return pattern;
}


// manipulates the object directly
function convertOldGestureFormatToNewFormat (gestureObj) {
  if (gestureObj.gesture && typeof gestureObj.gesture === "string") {
    gestureObj.pattern = convertDirectionsToPattern(gestureObj.gesture);
    delete gestureObj.gesture;
  }

  if (gestureObj.command && typeof gestureObj.command === "string") {
    gestureObj.command = {
      name: gestureObj.command
    };
  }

  if (gestureObj.settings) {
    gestureObj.command.settings = gestureObj.settings
    delete gestureObj.settings;
  }

  migrateFocusTabCommandSettings(gestureObj.command);
}


// manipulates the object directly
function convertOldCommandFormatToNewFormat (commandObj) {
  if (commandObj.command && typeof commandObj.command === "string") {
    commandObj.name = commandObj.command;
    delete commandObj.command;
  }

  migrateFocusTabCommandSettings(commandObj);
}


function migrateFocusTabCommandSettings (commandObj) {
  const affectedCommands = ["FocusRightTab", "FocusLeftTab"];
  if (commandObj && affectedCommands.includes(commandObj.name)) {

    if (!commandObj.settings) {
      commandObj.settings = {
        "cycling": true,
        "excludeDiscarded": false
      }
    }
    else if (!("cycling" in commandObj.settings)) commandObj.settings.cycling = true;
  }
}


// requires a version in the format of x.x.x
// cheks if v1 is a previous version of v2
function isPreviousVersion (v1, v2) {
  // convert version strings to array
  v1 = v1.split(".").map(Number);
  v2 = v2.split(".").map(Number);

  for (let i = 0; i < v1.length; i++) {
    if (v1[i] === v2[i]) continue;
    else if (v1[i] < v2[i]) return true;
    else break;
  }
  return false;
}


// add missing setting to all commands
function addSettingToCommand (configManager, commandName, settingName, defaultValue) {
  const addIfMissing = (command) => {
    if ((command?.name === commandName && !command?.settings?.hasOwnProperty(settingName))) {
      if (!command.hasOwnProperty("settings")) {
        command.settings = {};
      }
      command.settings[settingName] = defaultValue;
    }
  }

  const config = configManager.get();

  for (const gesture of config.Gestures) {
    addIfMissing(gesture.command);
  }
  addIfMissing(config.Settings.Rocker.leftMouseClick);
  addIfMissing(config.Settings.Rocker.rightMouseClick);
  addIfMissing(config.Settings.Wheel.wheelUp);
  addIfMissing(config.Settings.Wheel.wheelDown);

  configManager.set(config);
}