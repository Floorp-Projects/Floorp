# Set up to build Firefox Developer Tools

Since the tools are part of Firefox, we need to get and build Firefox's source code.

These are the steps we're going to look at:

* [Installing Mercurial](#installing-mercurial)
* [Getting the code](#getting-the-code)
* [Building and running locally](#building-and-running-locally)
  * [Rebuilding](#rebuilding)
  * [Artifact builds](#building-even-faster-with-artifact-builds) for even faster builds
  * [Maybe you don't even need to build](#maybe-you-dont-even-need-to-build)

## Installing Mercurial

Firefox's source code is hosted on [a Mercurial repository](https://hg.mozilla.org/mozilla-central/).

Please [install Mercurial](https://www.mercurial-scm.org/) if it's not already installed in your system. Its website provides [files for downloading](https://www.mercurial-scm.org/downloads) and also instructions for the most popular package managers. For example, if using Homebrew in macOS:

```bash
brew install mercurial
```

Or in Debian/Ubuntu Linux with apt-get:

```bash
apt-get install mercurial
```

## Getting the code

This will take a long time, because the repository is **B&nbsp;&nbsp;I&nbsp;&nbsp;G** (Firefox is more than 10 years old and there are lots of commits every day!). So please be patient.

You will also need a lot of disk space (about ~40Gb free).

```bash
cd ~ # or the folder where you store your projects, for example ~/projects
hg clone http://hg.mozilla.org/mozilla-central
```

## Building and running locally

Although DevTools is written in JavaScript, the core of Firefox is not—we need tools such as compilers to build the C/C++/Rust code and turn it into binaries that our computer can run.

If you're a web developer, you might not be familiar with these tools, but fortunately, the Firefox team has made a very good job of automating the process of installing them with bootstrap scripts, and putting [documentation](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build) together. You don't *need* to read it now; as we'll provide a simplified way to build the code, but if you have any issues, please refer to the documentation.

Try building Firefox and downloading dependencies by running the following commands:

```bash
./mach bootstrap
```

This script might ask you questions, so it's recommended to keep an eye on the terminal while it's running—otherwise it will never finish!

After it finishes, you might be asked to add some lines to a `mozconfig` file. Create this file in the repository folder (i.e. in `~/mozilla-central`), then add the lines.

Then run this:

```bash
./mach configure
./mach build
```

**Note:** if using Windows, you might need to type the commands without the `./`:

```bash
mach bootstrap
mach configure
mach build
```

If your system needs additional dependencies installed (for example, Python, or a compiler, etc) the above commands might fail, and various diagnostic messages will be printed to your screen. Follow their advice and then try running the command that failed again, until the three of them complete successfully.

Some error messages can be quite cryptic. It is a good idea to consult the [documentation](https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build) specific to the platform you're using. Sometimes searching in the internet for the specific error message you get can help, and you can also [get in touch](https://firefox-dev.tools/#getting-in-touch) if you get stuck.

Once you complete a first successful build, you should be able to build again by running only this command:

```bash
./mach build
```

By the way, building takes a long time (specially on slow computers).

### Running your own compiled version of Firefox

To run the Firefox you just compiled:

```bash
./mach run
```

This will run using an empty temporary profile which is discarded when you close the browser. We will look into [persistent development profiles later](./development-profiles.md). But first...

⭐️  **Time for some congratulations!** You managed to get Firefox's code, build tools and dependencies, and just run your very own copy of Firefox! Well done! ⭐   ️ 

### Rebuilding

<!--TODO: it would be valuable to explain how to pull changes! -->

Suppose you pulled the latest changes from the remote repository (or made some changes, to experiment and see what happens) and want to build again.

You can ask the `mach` script to build only changed files:

```bash
./mach build faster
```

This should be faster (a matter of seconds).

Sometimes, if you haven't updated in a while, you'll be told that you need to *clobber*, or basically delete precompiled stuff and start from scratch, because there are too many changes. The way to do it is:

```bash
./mach clobber
```

It is a bit tedious to do this manually, but fortunately you can add an entry to `mozconfig` to have this done automatically for you each time it's required. Add this and save the file:

```
# Automatically clobber when an incremental build is not possible
mk_add_options AUTOCLOBBER=1
```

### Building even faster with artifact builds

It is unusual to modify C/C++ code when working on DevTools. This means that we can use *artifact builds*. This method downloads prebuilt binary components, and then the build process becomes faster.

Add the following content to `mozconfig`:

```bash
# Automatically download and use compiled C++ components:
ac_add_options --enable-artifact-builds
 
# Write build artifacts to:
mk_add_options MOZ_OBJDIR=./objdir-frontend
```

And then you can follow the normal build process again (only *faster*!)

**Note**: On macOS you might want to use `MOZ_OBJDIR=./objdir-frontend.noindex` instead. Using the `.noindex` file extension prevents the Spotlight from indexing your `objdir`, which is slow.

For more information on aspects such as technical limitations of artifact builds, read the [Artifact Builds](https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Artifact_builds) page.

## Maybe you don't even need to build

Working in DevTools generally involves editing JavaScript files only. This means that often you don't even need to run `./mach build`.

Instead, you just need to save the files you modified, and restart Firefox. To achieve this, you *could* quit Firefox, and reopen it again with `./mach run`. Or you could use the `Restart (Developer)` option that is available under the `File` menu in local builds (or its respective shortcut).

![Image of Restart (Developer) option under File menu](restart.png)

That would be equivalent to the following pseudocode, performed manually:

```bash
# 1. Build
./mach build
# 2. Run
./mach run
# 3. you try out things in the browser that opens
# 4. fully close the browser, e.g. ⌘Q in MacOS
# 5. edit JS files on the `devtools` folder, save
# 6. Back to step 2!
./mach run
```

