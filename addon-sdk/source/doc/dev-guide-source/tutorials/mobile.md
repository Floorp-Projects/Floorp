<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<div class="warning">Developing add-ons for Firefox Mobile is still
an experimental feature of the SDK. Although the SDK modules used are
stable, the setup instructions and cfx commands are likely to change.
</div>

# Developing for Firefox Mobile #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

Mozilla has recently decided to
[reimplement the UI for Firefox Mobile on Android](http://starkravingfinkle.org/blog/2011/11/firefox-for-android-native-android-ui/)
 using native Android widgets instead of XUL. With the add-on SDK you
can develop add-ons that run on this new version of Firefox Mobile as
well as on the desktop version of Firefox.

You can use the same code to target both desktop Firefox and Firefox
Mobile, and just specify some extra options to `cfx run`, `cfx test`,
and `cfx xpi` when targeting Firefox Mobile.

Right now not all modules are fully functional, but we're working on adding
support for more modules.
The [tables at the end of this guide](dev-guide/tutorials/mobile.html#modules-compatibility) list the modules that are currently supported on Firefox Mobile.

This tutorial explains how to run SDK add-ons on an Android
device connected via USB to your development machine.
We'll use the
[Android Debug Bridge](http://developer.android.com/guide/developing/tools/adb.html)
(adb) to communicate between the Add-on SDK and the device.

<img class="image-center" src="static-files/media/mobile-setup-adb.png"/>

It's possible to use the
[Android emulator](http://developer.android.com/guide/developing/tools/emulator.html)
to develop add-ons for Android without access to a device, but it's slow,
so for the time being it's much easier to use the technique described
below.

## Setting up the Environment ##

First you'll need an
[Android device capable of running the native version of
Firefox Mobile](https://wiki.mozilla.org/Mobile/Platforms/Android#System_Requirements).
Then:

* install the
[Nightly build of the native version of Firefox Mobile](https://wiki.mozilla.org/Mobile/Platforms/Android#Download_Nightly)
on the device.
* [enable USB debugging on the device (step 3 of this link only)](http://developer.android.com/guide/developing/device.html#setting-up)

On the development machine:

* install version 1.5 or higher of the Add-on SDK
* install the correct version of the
[Android SDK](http://developer.android.com/sdk/index.html)
for your device
* using the Android SDK, install the
[Android Platform Tools](http://developer.android.com/sdk/installing.html#components)

Next, attach the device to the development machine via USB.

Now open up a command shell. Android Platform Tools will have
installed `adb` in the "platform-tools" directory under the directory
in which you installed the Android SDK. Make sure the "platform-tools"
directory is in your path. Then type:

<pre>
adb devices
</pre>

You should see some output like:

<pre>
List of devices attached
51800F220F01564 device
</pre>

(The long hex string will be different.)

If you do, then `adb` has found your device and you can get started.

## Running Add-ons on Android ##

You can develop your add-on as normal, as long as you restrict yourself
to the supported modules.

When you need to run the add-on, first ensure that Firefox is not running
on the device. Then execute `cfx run` with some extra options:

<pre>
cfx run -a fennec-on-device -b /path/to/adb --mobile-app fennec --force-mobile
</pre>

See ["cfx Options for Mobile Development"](dev-guide/tutorials/mobile.html#cfx-options)
for the details of this command.

In the command shell, you should see something like:

<pre>
Launching mobile application with intent name org.mozilla.fennec
Pushing the addon to your device
Starting: Intent { act=android.activity.MAIN cmp=org.mozilla.fennec/.App (has extras) }
--------- beginning of /dev/log/main
--------- beginning of /dev/log/system
Could not read chrome manifest 'file:///data/data/org.mozilla.fennec/chrome.manifest'.
info: starting
info: starting
zerdatime 1329258528988 - browser chrome startup finished.
</pre>

This will be followed by lots of debug output.

On the device, you should see Firefox launch with your add-on installed.

`console.log()` output from your add-on is written to the command shell,
just as it is in desktop development. However, because there's a
lot of other debug output in the shell, it's not easy to follow.
The command `adb logcat` prints `adb`'s log, so you can filter the
debug output after running the add-on. For example, on Mac OS X
or Linux you can use a command like:

<pre>
adb logcat | grep info:
</pre>

Running `cfx test` is identical:

<pre>
cfx test -a fennec-on-device -b /path/to/adb --mobile-app fennec --force-mobile
</pre>

## <a name="cfx-options">cfx Options for Mobile Development</a> ##

As you see in the quote above, `cfx run` and `cfx test` need four options to
work on Android devices.

<table>
<colgroup>
<col width="30%">
<col width="70%">
</colgroup>

<tr>
  <td>
    <code>-a fennec-on-device</code>
  </td>
  <td>
    This tells the Add-on SDK which application will host the
    add-on, and should be set to "fennec-on-device" when running
    an add-on on Firefox Mobile on a device.
  </td>
</tr>
<tr>
  <td>
    <code>-b /path/to/adb</code>
  </td>
  <td>
    <p>As we've seen, <code>cfx</code> uses the Android Debug Bridge (adb)
    to communicate with the Android device. This tells <code>cfx</code>
    where to find the <code>adb</code> executable.</p>
    <p>You need to supply the full path to the <code>adb</code> executable.</p>
  </td>
</tr>
<tr>
  <td>
    <code>--mobile-app</code>
  </td>
  <td>
    <p>This is the name of the <a href="http://developer.android.com/reference/android/content/Intent.html">
    Android intent</a>. Its value depends on the version of Firefox Mobile
    that you're running on the device:</p>
    <ul>
      <li><code>fennec</code>: if you're running Nightly</li>
      <li><code>fennec_aurora</code>: if you're running Aurora</li>
      <li><code>firefox_beta</code>: if you're running Beta</li>
      <li><code>firefox</code>: if you're running Release</li>
    </ul>
    <p>If you're not sure, run a command like this (on OS X/Linux, or the equivalent on Windows):</p>
    <pre>adb shell pm list packages | grep mozilla</pre>
    <p>You should see "package" followed by "org.mozilla." followed by a string.
    The final string is the name you need to use. For example, if you see:</p>
    <pre>package:org.mozilla.fennec</pre>
    <p>...then you need to specify:</p>
    <pre>--mobile-app fennec</pre>
    <p>This option is not required if you have only one Firefox application
    installed on the device.</p>
  </td>
</tr>
<tr>
  <td>
    <code>--force-mobile</code>
  </td>
  <td>
    <p>This is used to force compatibility with Firefox Mobile, and should
    always be used when running on Firefox Mobile.</p>
  </td>
</tr>
</table>

## Packaging Mobile Add-ons ##

To package a mobile add-on as an XPI, use the command:

<pre>
cfx xpi --force-mobile
</pre>

Actually installing the XPI on the device is a little tricky. The easiest way is
probably to copy the XPI somewhere on the device:

<pre>
adb push my-addon.xpi /mnt/sdcard/
</pre>

Then open Firefox Mobile and type this into the address bar:

<pre>
file:///mnt/sdcard/my-addon.xpi
</pre>

The browser should open the XPI and ask if you
want to install it.

Afterwards you can delete it using `adb` as follows:

<pre>
adb shell
cd /mnt/sdcard
rm my-addon.xpi
</pre>

<a name="modules-compatibility"></a>
## Module Compatibility

Modules not yet supported in Firefox Mobile are
<span class="unsupported-on-mobile">**marked**</span> in the tables below.

### High-Level APIs ###

 - [**addon-page**](modules/sdk/addon-page.html)
 - [base64](modules/sdk/clipboard.html)
 - [**clipboard**](modules/sdk/clipboard.html)
 - [**context-menu**](modules/sdk/context-menu.html)
 - [hotkeys](modules/sdk/hotkeys.html)
 - [indexed-db](modules/sdk/indexed-db.html)
 - [l10n](modules/sdk/l10n.html)
 - [notifications](modules/sdk/notifications.html)
 - [page-mod](modules/sdk/notifications.html)
 - [page-worker](modules/sdk/page-worker.html)
 - [**panel**](modules/sdk/panel.html)
 - [passwords](modules/sdk/passwords.html)
 - [**private-browsing**](modules/sdk/private-browsing.html)
 - [querystring](modules/sdk/querystring.html)
 - [request](modules/sdk/request.html)
 - [**selection**](modules/sdk/selection.html)
 - [self](modules/sdk/self.html)
 - [simple-prefs](modules/sdk/simple-prefs.html)
 - [simple-storage](modules/sdk/simple-storage.html)
 - [system](modules/sdk/system.html)
 - [tabs](modules/sdk/tabs.html)
 - [timers](modules/sdk/timers.html)
 - [url](modules/sdk/url.html)
 - [**widget**](modules/sdk/widget.html)
 - [windows](modules/sdk/windows.html)

### Low-Level APIs ###

 - [/loader](modules/toolkit/loader.html)
 - [chrome](dev-guide/tutorials/chrome.html)
 - [console/plain-text](modules/sdk/console/plain-text.html)
 - [console/traceback](modules/sdk/console/traceback.html)
 - [**content/content**](modules/sdk/content/content.html)
 - [content/loader](modules/sdk/content/loader.html)
 - [**content/symbiont**](modules/sdk/content/symbiont.html)
 - [**content/worker**](modules/sdk/content/worker.html)
 - core/disposable
 - [core/heritage](modules/sdk/core/heritage.html)
 - [core/namespace](modules/sdk/core/namespace.html)
 - [core/promise](modules/sdk/core/promise.html)
 - [deprecated/api-utils](modules/sdk/deprecated/api-utils.html)
 - [deprecated/app-strings](modules/sdk/deprecated/app-strings.html)
 - [deprecated/cortex](modules/sdk/deprecated/cortex.html)
 - [deprecated/errors](modules/sdk/deprecated/errors.html)
 - [deprecated/events](modules/sdk/deprecated/events.html)
 - [deprecated/light-traits](modules/sdk/deprecated/light-traits.html)
 - deprecated/list
 - [deprecated/observer-service](modules/sdk/deprecated/observer-service.html)
 - [**deprecated/tab-browser**](modules/sdk/deprecated/tab-browser.html)
 - [deprecated/traits](modules/sdk/deprecated/traits.html)
 - [**deprecated/window-utils**](modules/sdk/deprecated/window-utils.html)
 - dom/events
 - [event/core](modules/sdk/event/core.html)
 - [event/target](modules/sdk/event/target.html)
 - [frame/hidden-frame](modules/sdk/frame/hidden-frame.html)
 - [frame/utils](modules/sdk/frame/utils.html)
 - [io/byte-streams](modules/sdk/io/byte-streams.html)
 - [io/file](modules/sdk/io/file.html)
 - [io/text-streams](modules/sdk/io/text-streams.html)
 - keyboard/observer
 - keyboard/utils
 - lang/functional
 - lang/type
 - [loader/cuddlefish](modules/sdk/loader/cuddlefish.html)
 - [loader/sandbox](modules/sdk/loader/sandbox.html)
 - [net/url](modules/sdk/net/url.html)
 - [net/xhr](modules/sdk/net/xhr.html)
 - [platform/xpcom](modules/sdk/platform/xpcom.html)
 - [preferences/service](modules/sdk/preferences/service.html)
 - [system/environment](modules/sdk/system/environment.html)
 - [system/events](modules/sdk/system/events.html)
 - system/globals
 - [system/runtime](modules/sdk/system/runtime.html)
 - [system/unload](modules/sdk/system/unload.html)
 - [system/xul-app](modules/sdk/system/xul-app.html)
 - [test/assert](modules/sdk/test/assert.html)
 - [test/harness](modules/sdk/test/harness.html)
 - [test/httpd](modules/sdk/test/httpd.html)
 - [test/runner](modules/sdk/test/runner.html)
 - test/tmp-file
 - util/array
 - [util/collection](modules/sdk/util/collection.html)
 - [util/deprecate](modules/sdk/util/deprecate.html)
 - [util/list](modules/sdk/util/list.html)
 - [util/match-pattern](modules/sdk/util/match-pattern.html)
 - util/registry
 - [util/uuid](modules/sdk/util/uuid.html)
 - [window/utils](modules/sdk/window/utils.html)
