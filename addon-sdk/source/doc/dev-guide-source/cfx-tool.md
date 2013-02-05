<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# cfx #

The `cfx` command-line tool gives you access to the SDK documentation and
development servers as well as testing, running, and building add-ons.
`cfx` usage is:

<pre>
  cfx [options] command [command-specific options]
</pre>

"Options" are global options applicable to the tool itself or to all
commands (for example `--help`). `cfx` supports the following global options:

<pre>
  -h, --help        - show a help message and exit
  -v, --verbose     - enable lots of output
</pre>

"Command-specific options" are only
applicable to a subset of the commands.

## Supported Commands ##

### cfx docs ###

This command displays the documentation for the SDK. The documentation is
shipped with the SDK in [Markdown](http://daringfireball.net/projects/markdown/)
format. The first time this command is executed, and any time after the
Markdown files on disk have changed, `cfx docs` will generate a set of HTML
pages from them and launch a web browser to display them. If the Markdown files
haven't changed, `cfx docs` just launches a browser initialized to the set of
generated pages.

To regenerate the documentation associated with a single file, you can
specify the file as an argument. For example:

<pre>
  cfx docs doc/dev-guide-source/addon-development/cfx-tool.md 
</pre>

This command will regenerate only the HTML page you're reading.
This is useful if you're iteratively editing a single file, and don't want to wait for cfx to
regenerate the complete documentation tree.

### cfx init ####
Create a new directory called "my-addon", change into it, and run `cfx init`.

This command will create an skeleton add-on, as a starting point for your
own add-on development, with the following file structure:

<ul class="tree">
  <li>my-addon
    <ul>
    <li>data</li>
    <li>docs
      <ul><li>main.md</li></ul>
    </li>
    <li>lib
      <ul><li>main.js</li></ul>
    </li>
    <li>package.json</li>
    <li>README.md</li>
    <li>tests
      <ul><li>test-main.js</li></ul>
    </li>
    </ul>
  </li>
</ul>

<div style="clear:both"></div>

### cfx run ###

This command is used to run the add-on. Called with no options it looks for a
file called `package.json` in the current directory, loads the corresponding
add-on, and runs it under the version of Firefox it finds in the platform's
default install path.

#### Supported Options #####

You can point `cfx run` at a different `package.json` file using the
`--pkgdir` option, and pass arguments to your add-on using the
`--static-args` option.

You can specify a different version of the
<a href="dev-guide/glossary.html#host-application">host application</a>
using the `--binary` option, passing in the path to the application binary to
run. The path may be specified as a full path or may be relative to the current
directory. But note that the version must be 4.0b7 or later.

`cfx run` runs the host application with a new
[profile](http://support.mozilla.com/en-US/kb/profiles). You can specify an
existing profile using the `--profiledir` option, and this gives you access to
that profile's history, bookmarks, and other add-ons. This enables you to run
your add-on alongside debuggers like [Firebug](http://getfirebug.com/).
See <a href="dev-guide/cfx-tool.html#profiledir">
"Using --profiledir"</a> for more information.

<table>
<colgroup>
<col width="30%">
<col width="70%">
</colgroup>

<tr>
  <td>
    <code>-b BINARY, --binary=BINARY</code>
  </td>
  <td>
    Use the host application binary specified in BINARY. BINARY may be specified as
    a full path or as a path relative to the current directory.
  </td>
</tr>

<tr>
  <td>
    <code>--binary-args=CMDARGS</code>
  </td>
  <td>
    <p>Pass <a href="http://kb.mozillazine.org/Command_line_arguments">extra
    arguments</a> to the binary being executed (for example, Firefox).</p>
    <p>For example, to pass the
    <code>-jsconsole</code> argument to Firefox, which will launch the
    <a href="https://developer.mozilla.org/en/Error_Console">JavaScript
    Error Console</a>, try the following:</p>
    <pre>cfx run --binary-args -jsconsole</pre>
    <p>To pass multiple arguments, or arguments containing spaces, quote them:</p>
    <pre>cfx run --binary-args '-url "www.mozilla.org" -jsconsole'</pre>
    </td>
</tr>

<tr>
  <td>
    <code>--extra-packages=EXTRA_PACKAGES</code>
  </td>
  <td>
    Extra packages to include, specified as a comma-separated list of package
    names.
  </td>
</tr>

<tr>
  <td>
    <code>-g CONFIG, --use-config=CONFIG</code>
  </td>
  <td>
    Pass a set of options by
    <a href="dev-guide/cfx-tool.html#configurations">referencing a named configuration</a>.
  </td>
</tr>

<tr>
  <td>
    <code>-p PROFILEDIR, --profiledir=PROFILEDIR</code>
  </td>
  <td>
    <p>Use an existing
    <a href="http://support.mozilla.com/en-US/kb/profiles">profile</a>
    located in PROFILEDIR. PROFILEDIR may be specified as
    a full path or as a path relative to the current directory.</p>

    <p>See <a href="dev-guide/cfx-tool.html#profiledir">
    "Using --profiledir"</a> for more information.
  </td>
</tr>

<tr>
  <td>
    <code>--pkgdir=PKGDIR</code>
  </td>
  <td>
    Use an add-on located in PKGDIR. PKGDIR may be specified as
    a full path or as a path relative to the current directory.
  </td>
</tr>

<tr>
  <td>
    <code>--static-args=STATIC_ARGS</code>
  </td>
  <td>
    <a href="dev-guide/cfx-tool.html#arguments">Pass arguments to your add-on</a>,
    in JSON format.
  </td>
</tr>

</table>

#### Experimental Options ####

<table>
<colgroup>
<col width="30%">
<col width="70%">
</colgroup>

<tr>
  <td>
    <code>-a APP, --app=APP</code>
  </td>
  <td>
    By default, <code>cfx run</code> uses Firefox as the
    <a href="dev-guide/glossary.html#host-application">host application</a>.
    This option enables you to select a different host. You can specify
    "firefox", "xulrunner", "fennec", or "thunderbird". But note that at
    present only Firefox is supported.
  </td>
</tr>

<tr>
  <td>
    <code>--no-run</code>
  </td>
  <td>
    <p>With this option <code>cfx</code> will not execute the command, but
    will print out the command that it would have used to execute the
    command.</p>
    <p>For example, if you type:</p>
    <pre>
cfx run ---no-run</pre>
    <p>you will see something like:</p>
    <pre>
To launch the application, enter the following command:
 /path/to/firefox/firefox-bin -profile
 /path/to/profile/tmpJDNlP6.mozrunner -foreground -no-remote</pre>
    <p>This enables you to run the add-on without going through
    <code>cfx</code>, which might be useful if you want to run it
    inside a debugger like GDB.</p>
  </td>
</tr>

<tr>
  <td>
    <code>--templatedir=TEMPLATEDIR</code>
  </td>
  <td>
    The <code>cfx run</code> command constructs the add-on using a extension
    template which you can find under the SDK root, in
    <code>app-extension</code>.
    Use the <code>--templatedir</code> option to specify a different template.
    TEMPLATEDIR may be specified as a full path or as a path relative to the
    current directory.
  </td>
</tr>

</table>

#### Internal Options ####

<table>
<colgroup>
<col width="30%">
<col width="70%">
</colgroup>

<tr>
  <td>
    <code>--addons=ADDONS</code>
  </td>
  <td>
    Paths of add-ons to install, comma-separated. ADDONS may be specified as
    a full path or as a path relative to the current directory.
  </td>
</tr>

<tr>
  <td>
    <code>--e10s</code>
  </td>
  <td>
    If this option is set then the add-on runs in a separate process.
    This option is currently not implemented.
  </td>
</tr>

<tr>
  <td>
    <code>--keydir=KEYDIR</code>
  </td>
  <td>
    Supply a different location for
    <a href="dev-guide/guides/program-id.html">signing keys</a>.
    KEYDIR may be specified as a full path or as a path relative to the
    current directory.
  </td>
</tr>

</table>

### cfx test ###

Run available tests for the specified package.

<span class="aside">Note the hyphen after "test" in the module name.
`cfx test` will include a module called "test-myCode.js", but will exclude
modules called "test_myCode.js" or "testMyCode.js".</span>

Called with no options this command will look for a file called `package.json`
in the current directory. If `package.json` exists, `cfx` will load the
corresponding add-on, load from the `tests` directory
any modules that start with the word `test-` and run the unit tests
they contain.

See the
[tutorial on unit testing](dev-guide/tutorials/unit-testing.html) and the
[reference documentation for the `assert` module](modules/sdk/test/assert.html)
for details.

#### Supported Options #####

As with `cfx run` you can use options to control which host application binary
version to use, and to select a profile.

You can also control which tests are run: you
can test dependent packages, filter the tests by name and run tests multiple
times.

<table>
<colgroup>
<col width="30%">
<col width="70%">
</colgroup>

<tr>
  <td>
    <code>-b BINARY, --binary=BINARY</code>
  </td>
  <td>
    Use the host application binary specified in BINARY. BINARY may be specified as
    a full path or as a path relative to the current directory.
  </td>
</tr>

<tr>
  <td>
    <code>--binary-args=CMDARGS</code>
  </td>
  <td>
    <p>Pass <a href="http://kb.mozillazine.org/Command_line_arguments">extra
    arguments</a> to the binary being executed (for example, Firefox).</p>
    <p>For example, to pass the
    <code>-jsconsole</code> argument to Firefox, which will launch the
    <a href="https://developer.mozilla.org/en/Error_Console">JavaScript
    Error Console</a>, try the following:</p>
    <pre>cfx run --binary-args -jsconsole</pre>
    <p>To pass multiple arguments, or arguments containing spaces, quote them:</p>
    <pre>cfx run --binary-args '-url "www.mozilla.org" -jsconsole'</pre>
    </td>
</tr>

<tr>
  <td>
    <code>--dependencies</code>
  </td>
  <td>
    Load and run any tests that are included with modules that your package
    depends on.
    <br>
    For example: if your add-on depends on modules from the SDK, then
    <code>cfx</code> will run the unit tests for the SDK's modules as well
    as yours.
  </td>
</tr>

<tr>
  <td>
    <code>-f FILENAME[:TESTNAME], --filter=FILENAME[:TESTNAME]</code>
  </td>
  <td>
    Only run tests whose filenames match FILENAME and
    optionally match TESTNAME, both regexps (test, testall, testex, testpkgs)
    <br>
    For example: if you specify <code>--filter data</code>, then
    <code>cfx</code> will only run tests in those modules whose name contain
    the string "data".
  </td>
</tr>

<tr>
  <td>
    <code>-g CONFIG, --use-config=CONFIG</code>
  </td>
  <td>
    Pass a set of options by
    <a href="dev-guide/cfx-tool.html#configurations">referencing a named configuration</a>.
  </td>
</tr>

<tr>
  <td>
    <code>-p PROFILEDIR, --profiledir=PROFILEDIR</code>
  </td>
  <td>
    <p>Use an existing
    <a href="http://support.mozilla.com/en-US/kb/profiles">profile</a>
    located in PROFILEDIR. PROFILEDIR may be specified as
    a full path or as a path relative to the current directory.</p>

    <p>See <a href="dev-guide/cfx-tool.html#profiledir">
    "Using --profiledir"</a> for more information.
  </td>
</tr>

<tr>
  <td>
    <code>--times=ITERATIONS</code>
  </td>
  <td>
    Execute tests ITERATIONS number of times.
  </td>
</tr>

</table>

#### Experimental Options ####

<table>
<colgroup>
<col width="30%">
<col width="70%">
</colgroup>

<tr>
  <td>
    <code>-a APP, --app=APP</code>
  </td>
  <td>
    By default, <code>cfx test</code> uses Firefox as the
    <a href="dev-guide/glossary.html#host-application">host application</a>.
    This option enables you to select a different host. You can specify
    "firefox", "xulrunner", "fennec", or "thunderbird". But note that at
    present only Firefox is supported.
  </td>
</tr>

<tr>
  <td>
    <code>--no-run</code>
  </td>
  <td>
    <p>With this option <code>cfx</code> will not execute the command, but
    will print out the command that it would have used to execute the
    command.</p>
    <p>For example, if you type:</p>
    <pre>
cfx run ---no-run</pre>
    <p>you will see something like:</p>
    <pre>
To launch the application, enter the following command:
 /path/to/firefox/firefox-bin -profile
 /path/to/profile/tmpJDNlP6.mozrunner -foreground -no-remote</pre>
    <p>This enables you to run the add-on without going through
    <code>cfx</code>, which might be useful if you want to run it
    inside a debugger like GDB.</p>
  </td>
</tr>

<tr>
  <td>
    <code>--use-server</code>
  </td>
  <td>
    Run tests using a server previously started with <code>cfx develop</code>.
  </td>
</tr>

</table>

#### Internal Options ####

<table>
<colgroup>
<col width="30%">
<col width="70%">
</colgroup>

<tr>
  <td>
    <code>--addons=ADDONS</code>
  </td>
  <td>
    Paths of add-ons to install, comma-separated.
    ADDONS may be specified as full paths or relative to the
    current directory.
  </td>
</tr>

<tr>
  <td>
    <code>--e10s</code>
  </td>
  <td>
    If this option is set then the add-on runs in a separate process.
    This option is currently not implemented.
  </td>
</tr>

<tr>
  <td>
    <code>--keydir=KEYDIR</code>
  </td>
  <td>
    Supply a different location for
    <a href="dev-guide/guides/program-id.html">signing keys</a>.
    KEYDIR may be specified as a full path or as a path relative to the
    current directory.
  </td>
</tr>

<tr>
  <td>
    <code>--logfile=LOGFILE</code>
  </td>
  <td>
    Log console output to the file specified by LOGFILE.
    LOGFILE may be specified as a full path or as a path relative to the
    current directory.
  </td>
</tr>

<tr>
  <td>
    <code>--profile-memory=PROFILEMEMORY</code>
  </td>
  <td>
    If this option is given and PROFILEMEMORY is any non-zero integer, then
    <code>cfx</code> dumps detailed memory usage information to the console
    when the tests finish.
  </td>
</tr>

<tr>
  <td>
    <code>--test-runner-pkg=TEST_RUNNER_PKG</code>
  </td>
  <td>
    Name of package containing test runner program. Defaults to
    <code>test-harness</code>.
  </td>
</tr>

</table>

### cfx xpi ###

This tool is used to package your add-on as an
[XPI](https://developer.mozilla.org/en/XPI) file, which is the install file
format for Mozilla add-ons.

Called with no options, this command looks for a file called `package.json` in
the current directory and creates the corresponding XPI file.

Once you have built an XPI file you can distribute your add-on by submitting
it to [addons.mozilla.org](http://addons.mozilla.org).

#### updateURL and updateLink ####

If you choose to host the XPI yourself you should enable the host application
to find new versions of your add-on.

To do this, include a URL in the XPI called the
[updateURL](https://developer.mozilla.org/en/install_manifests#updateURL): the
host application will go here to get information about updates. At the
`updateURL` you host a file in the
[update RDF](https://developer.mozilla.org/en/extension_versioning,_update_and_compatibility#Update_RDF_Format)
format: among other things, this includes another URL called `updateLink` which
points to the updated XPI itself.

The `--update-link` and `--update-url` options simplify this process.
Both options take a URL as an argument.

The `--update-link` option builds an update RDF alongside the XPI, and embeds
the supplied URL in the update RDF as the value of `updateLink`.

The `--update-url` option embeds the supplied URL in the XPI file, as the value
of `updateURL`.

Note that as the [add-on documentation](https://developer.mozilla.org/en/extension_versioning,_update_and_compatibility#Securing_Updates)
explains, you should make sure the update procedure for your add-on is secure,
and this usually involves using HTTPS for the links.

So if we run the following command:

<pre>
  cfx xpi --update-link https://example.com/addon/latest
          --update-url https://example.com/addon/update_rdf
</pre>

`cfx` will create two files:

* an XPI file which embeds
`https://example.com/addon/update_rdf` as the value of `updateURL`
* an RDF file which embeds `https://example.com/addon/latest` as the value of
`updateLink`.

#### Supported Options ####

As with `cfx run` you can point `cfx` at a different `package.json` file using
the `--pkgdir` option. You can also embed arguments in the XPI using the
`--static-args` option: if you do this the arguments will be passed to your
add-on whenever it is run.

<table>
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>

<tr>
  <td>
    <code>--extra-packages=EXTRA_PACKAGES</code>
  </td>
  <td>
   Extra packages to include, specified as a comma-separated list of package
   names.
  </td>
</tr>

<tr>
  <td>
    <code>-g CONFIG, --use-config=CONFIG</code>
  </td>
  <td>
    Pass a set of options by
    <a href="dev-guide/cfx-tool.html#configurations">referencing a named configuration</a>.
  </td>
</tr>

<tr>
  <td>
    <code>--pkgdir=PKGDIR</code>
  </td>
  <td>
    Use an add-on located in PKGDIR.
    PKGDIR may be specified as a full path or as a path relative to the
    current directory.
  </td>
</tr>

<tr>
  <td>
    <code>--static-args=STATIC_ARGS</code>
  </td>
  <td>
    <a href="dev-guide/cfx-tool.html#arguments">Pass arguments to your add-on</a>,
    in JSON format.
  </td>
</tr>

<tr>
  <td>
    <code>--update-link=UPDATE_LINK</code>
  </td>
  <td>
    Build an
    <a href="https://developer.mozilla.org/en/extension_versioning,_update_and_compatibility#Update_RDF_Format">update RDF</a>
    alongside the XPI file, and embed the URL supplied in UPDATE_LINK in it as
    the value of <code>updateLink</code>.
  </td>
</tr>

<tr>
  <td>
    <code>--update-link=UPDATE_URL</code>
  </td>
  <td>
    Embed the URL supplied in UPDATE_URL in the XPI file, as the value
    of <code>updateURL</code>.
  </td>
</tr>

</table>

#### Experimental Options ####

<table>
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>

<tr>
  <td>
    <code>--templatedir=TEMPLATEDIR</code>
  </td>
  <td>
    The <code>cfx xpi</code> command constructs the add-on using a extension
    template which you can find under the SDK root, in
    <code>app-extension</code>.
    Use the <code>--templatedir</code> option to specify a different template.
    TEMPLATEDIR may be specified as a full path or as a path relative to the
    current directory.
  </td>
</tr>

</table>

#### Internal Options ####

<table>
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>

<tr>
  <td>
    <code>--keydir=KEYDIR</code>
  </td>
  <td>
    Supply a different location for
    <a href="dev-guide/guides/program-id.html">signing keys</a>.
    KEYDIR may be specified as a full path or as a path relative to the
    current directory.
  </td>
</tr>

</table>

## Experimental Commands ##

### cfx develop ###

This initiates an instance of a host application in development mode,
and allows you to pipe commands into it from another shell without
having to constantly restart it. Aside from convenience, for SDK
Platform developers this has the added benefit of making it easier to
detect leaks.

For example, in shell A, type:

<pre>
  cfx develop
</pre>

In shell B, type:

<pre>
  cfx test --use-server
</pre>

This will send `cfx test --use-server` output to shell A. If you repeat the
command in shell B, `cfx test --use-server` output will appear again in shell A
without restarting the host application.

`cfx develop` doesn't take any options.

## Internal Commands ##

### cfx sdocs ###

Executing this command builds a static HTML version of the SDK documentation
that can be hosted on a web server without the special application support
required by `cfx docs`.

#### Options ####

<table>
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>

<tr>
  <td>
    <code>--baseurl=BASEURL</code>
  </td>
  <td>
    The root of the static docs tree, for example:
    <code>http://example.com/sdk-docs/</code>.
  </td>
</tr>

</table>

### cfx testcfx ###

This will run a number of tests on the cfx tool, including tests against the
documentation. Use `cfx testcfx -v` for the specific list of tests.

This accepts the same options as `cfx test`.

### cfx testaddons ###

This will run a number of test add-ons that are packaged with the SDK.

This accepts the same options as `cfx test`.

### cfx testpkgs ###

This will test all of the available CommonJS packages. Note that the number
of tests run and their success depends on what application they are run
with, and which binary is used.

This accepts the same options as `cfx test`.

### cfx testex ###

This will test all available example code. Note that the number
of tests run and their success depends on what application they are run
with, and which binary is used.

This accepts the same options as `cfx test`.

### cfx testall ###

This will test *everything*: the cfx tool, all available CommonJS packages,
and all examples.

This accepts the same options as `cfx test`.

## <a name="profiledir">Using --profiledir</a> ##

By default, `cfx run` and `cfx test` use a new profile each time they
are executed. This means that any profile-specific data entered from
one run of `cfx` will not, by default, be available in the next run.

This includes, for example, any extra add-ons you installed, or your
history, or any data stored using the
[simple-storage](modules/sdk/simple-storage.html) API.

To make `cfx` use a specific profile, pass the `--profiledir` option,
specifying the path to the profile you wish to use.

If you give `--profiledir` a path to a nonexistent profile, `cfx`
will create a profile there for you. So you just have to make up
a path and name the first time, and keep using it:

<pre>
cfx run --profiledir="~/addon-dev/profiles/boogaloo"
</pre>

The path must contain at least one "/" (although you may specify
just "./dir").

## <a name="configurations">Using Configurations</a> ##

The `--use-config` option enables you to specify a set of options as a named
configuration in a file, then pass them to `cfx` by referencing the named set.

You define configurations in a file called `local.json` which should live
in the root directory of your SDK. Configurations are listed under a key called
"configs".

Suppose your the following `local.json` is as follows:

<pre>
  {
      "configs": {
          "ff40": ["-b", "/usr/bin/firefox-4.0"]
      }
  }
</pre>

You can run:

<pre>
  cfx test --use-config=ff40
</pre>

And it would be equivalent to:

<pre>
  cfx test -a firefox -b /usr/bin/firefox-4.0
</pre>

This method of defining configuration options can be used for all of the `run`,
build, and test tools. If "default" is defined in the `local.json` cfx will use
that configuration unless otherwise specified.

## <a name="arguments">Passing Static Arguments</a> ##

You can use the cfx `--static-args` option to pass arbitrary data to your
program.  This may be especially useful if you run cfx from a script.

The value of `--static-args` must be a JSON string.  The object encoded by the
JSON becomes the `staticArgs` property of the
[`system` module](modules/sdk/system.html).

The default value of
`--static-args` is `"{}"` (an empty object), so you don't have to worry about
checking whether `staticArgs` exists in `system`.

For example, if your add-on looks like this:

    var system = require("sdk/system");
    console.log(system.staticArgs.foo);

And you run cfx like this:

<pre>
  cfx run --static-args="{ \"foo\": \"Hello from the command line\" }"
</pre>

Then your console should contain this:

<pre>
info: my-addon: Hello from the command line
</pre>

The `--static-args` option is recognized by two of the package-specific
commands: `run` and `xpi`.  When used with the `xpi` command, the JSON is
packaged with the XPI's harness options and will therefore be used whenever the
program in the XPI is run.
