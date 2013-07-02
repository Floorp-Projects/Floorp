<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Installation #

## Prerequisites

To develop with the Add-on SDK, you'll need:

* [Python](http://www.python.org/) 2.5 or 2.6. Note that versions 3.0 and 3.1
  of Python are not supported. Make sure that Python is in your path.

* A [compatible version of Firefox](dev-guide/guides/firefox-compatibility.html).
That's either: the version of Firefox shipping at the time the SDK shipped,
or the Beta version of Firefox at the time the SDK shipped. See the
[SDK Release Schedule](https://wiki.mozilla.org/Jetpack/SDK_2012_Release_Schedule)
to map SDK releases to Firefox releases.

* The SDK itself: you can obtain the latest stable version of the SDK as a
[tarball](https://ftp.mozilla.org/pub/mozilla.org/labs/jetpack/jetpack-sdk-latest.tar.gz)
or a [zip file](https://ftp.mozilla.org/pub/mozilla.org/labs/jetpack/jetpack-sdk-latest.zip).
Alternatively, you can get the latest development version from its
[GitHub repository](https://github.com/mozilla/addon-sdk).

## Installation on Mac OS X / Linux ##

Extract the file contents wherever you choose, and navigate to the root
directory of the SDK with a shell/command prompt. For example:

<pre>
tar -xf addon-sdk.tar.gz
cd addon-sdk
</pre>

Then run if you're a Bash user (most people are):

<pre>
source bin/activate
</pre>

And if you're a non-Bash user, you should run:

<pre>
bash bin/activate
</pre>

Your command prompt should now have a new prefix containing the name of the
SDK's root directory:

<pre>
(addon-sdk)~/mozilla/addon-sdk >
</pre>

## Installation on Windows ##

Extract the file contents wherever you choose, and navigate to the root
directory of the SDK with a shell/command prompt. For example:

<pre>
7z.exe x addon-sdk.zip
cd addon-sdk
</pre>

Then run:

<pre>
bin\activate
</pre>

Your command prompt should now have a new prefix containing the full path to
the SDK's root directory:

<pre>
(C:\Users\mozilla\sdk\addon-sdk) C:\Users\Work\sdk\addon-sdk>
</pre>

## SDK Virtual Environment ##

The new prefix to your command prompt indicates that your shell has entered
a virtual environment that gives you access to the Add-on SDK's command-line
tools.

At any time, you can leave a virtual environment by running `deactivate`.

The virtual environment is specific to this particular command prompt. If you
close this command prompt, it is deactivated and you need to type
`source bin/activate` or `bin\activate` in a new command prompt to reactivate
it. If you open a new command prompt, the SDK will not be active in the new
prompt.

You can have multiple copies of the SDK in different locations on disk and
switch between them, or even have them both activated in different command
prompts at the same time.

### Making `activate` Permanent ###

All `activate` does is to set a number of environment variables for the
current command prompt, using a script located in the top-level `bin`
directory. By setting these variables permanently in your environment so
every new command prompt reads them, you can make activation permanent. Then
you don't need to type `activate` every time you open up a new command prompt.

Because the exact set of variables may change with new releases of the SDK,
it's best to refer to the activation scripts to determine which variables need
to be set. Activation uses different scripts and sets different variables for
bash environments (Linux and Mac OS X) and for Windows environments.

#### Windows ####

On Windows, `bin\activate` uses `activate.bat`, and you can make activation
permanent using the command line `setx` tool or the Control Panel.

#### Linux/Mac OS X ####

On Linux and Mac OS X, `source bin/activate` uses the `activate` bash
script, and you can make activation permanent using your `~/.bashrc`
(on Linux) or `~/.bashprofile` (on Mac OS X).

As an alternative to this, you can create a symbolic link to the `cfx`
program in your `~/bin` directory:

<pre>
ln -s PATH_TO_SDK/bin/cfx ~/bin/cfx
</pre>

## Sanity Check ##

Run this at your shell prompt:

<pre>
cfx
</pre>

It should produce output whose first line looks something like this, followed by
many lines of usage information:

<pre>
Usage: cfx [options] [command]
</pre>

This is the `cfx` command-line program.  It's your primary interface to the
Add-on SDK.  You use it to launch Firefox and test your add-on, package your
add-on for distribution, view documentation, and run unit tests.

## Problems? ##

Try the [Troubleshooting](dev-guide/tutorials/troubleshooting.html)
page.

## Next Steps ##

Next, take a look at the
[Getting Started With cfx](dev-guide/tutorials/getting-started-with-cfx.html)
tutorial, which explains how to create add-ons using the `cfx` tool.
