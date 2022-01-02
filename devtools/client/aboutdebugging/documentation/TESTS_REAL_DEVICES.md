# Running Tests with real USB connected devices for the new about:debugging

This document explains how to test with real USB connected devices.

## Tests overview

The tests that use a real device are located in `devtools/client/aboutdebugging/test/browser/`and the name of tests starts with `browser_aboutdebugging_real`. These are normal mochitest, but we need to setup the environment before starting tests.

## Setup environment
### Real device side
1. Enable USB debugging on your device
2. Launch Firefox
3. Enable USB debugging on your Firefox
4. Connect to your PC via USB

You can refer to https://developer.mozilla.org/en-US/docs/Tools/Remote_Debugging/Debugging_over_USB

### PC side
Setup the real device information to evaluate the validity in tests.

1. Copy a sample file which is located at `devtools/client/aboutdebugging/test/browser/real/usb-runtimes-sample.json` and rename it for example to `devtools/client/aboutdebugging/test/browser/real/local-usb-runtimes.json`.
2. Edit the file.

   This is a JSON file like below, write your real device information in here. This example indicates that there should be one USB device and should be displayed `Pixel 2` as device name and `Firefox Nightly` as short name on the sidebar of about:debugging. Regarding the other information, please see `Detail of config file` section of this document.

```
[
  {
    "sidebarInfo": {
      "deviceName": "Pixel 2",
      "shortName": "Firefox Nightly"
    },
    ...
  },
  ...
]
```

## Test
Normally, although we do test `./mach mochitest <path>`, to load the real device information created, specify the path as `USB_RUNTIME` environment variable, then do test.
For example, if the name of the saved file is `devtools/client/aboutdebugging/test/browser/real/local-usb-runtimes.json`, run all real device test with a command like the one below:

```
USB_RUNTIMES=devtools/client/aboutdebugging/test/browser/real/local-usb-runtimes.json ./mach mochitest devtools/client/aboutdebugging/test/browser/browser_aboutdebugging_real
```

If there is no `USB_RUNTIMES` environment variable, the tests will not run. This is also to avoid to run on try-server and so on.

## Detail of config file

```
[
  {
    "sidebarInfo": {
      "deviceName": "Pixel 2", // This should display as device name on the sidebar.
      "shortName": "Firefox Nightly" // This should display as short name on the sidebar.
    },
    "runtimeDetails": {
      "info": {
        "name": "Mozilla Nightly", // This should display on the runtime info of runtime page.
        "version": "64.0a1" // This should display on the runtime info of runtime page.
      }
    }
  }
  // Of course, you can append additional USB devices. Some tests can do with multiple devices.
]
```