## Table of contents

To set up `mozilla-central`, follow parts 1-6. Part 7 is for debugging common errors that may pop up during the process.

1) Before you start
2) Bootstrapping your modules
3) Cloning mozilla-central
4) Bootstrapping mozilla-central
5) Setting up arcanist and moz-phab
6) Making your first practice patch
7) Common errors

## Before you start

1) Install [Xcode](https://developer.apple.com/xcode/).
2) Install [Homebrew](https://brew.sh/).

## Bootstrapping your modules

1) Create a "src" directory for yourself under your home directory:
```
mkdir src && cd src
```

2) Download the [bootstrap.py](https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py) script and save it in your `src/` directory.

3) Run the bootstrap script with:
```
python bootstrap.py
```

4) Follow the prompts, which should be mostly Yes (`Y`). One of the prompts will ask you if you want to clone mozilla-unified. Leave the destination field empty and hit Enter.

## Cloning mozilla-central

In your `src` folder run the following:
```
hg clone https://hg.mozilla.org/mozilla-central/ mozilla-central
```

Your directory should now look something like this:
```
src
  |--- mozilla-central
  |--- bootstrap.py
```

Go ahead and open your cloned `mozilla-central` folder:
```
cd mozilla-central
```

## Bootstrapping mozilla-central

1. Run the command below to bootstrap your mozilla-central:
```
./mach bootstrap
```

2. At the end of the bootstrapping process, it will ask you to create a `mozconfig` file. Create a file named `mozconfig` in the `mozilla-central` folder, and paste the following. Afterwards, save that file:
```
ac_add_options --enable-artifact-builds
``` 

3. Run the command below to build mozilla-central
```
./mach build
```

4. If your build is successful, it should tell you that you can now run `./mach run`. Go ahead and do that to run Firefox!
```
./mach run
```

## Setting up arcanist and moz-phab

Follow these instructions here: https://moz-conduit.readthedocs.io/en/latest/arcanist-macos.html.

If at the end of the instructions you try to run `moz-phab -h` and it can't find the command, edit your `.bash_profile`:
```
sudo nano ~/.bash_profile
```

Enter your password, and then paste the string below into that file:
```
export PATH="$HOME/.mozbuild/arcanist/bin:$HOME/.mozbuild/moz-phab:$PATH"
```

Restart your terminal, and `moz-phab -h` should now work as expected.

## Making your first practice patch

To verify that everything works correctly, try submitting a practice patch. These steps are primarily based off of https://moz-conduit.readthedocs.io/en/latest/arcanist-user.html.

Before proceeding, make sure to create a [Phabricator account](https://phabricator.services.mozilla.com). 

1) Go into your mozilla-central folder and setup arc:
```
cd mozilla-central
arc install-certificate
```

2) Using your editor, create a test file named `testfile`.

3) Check to see if your `testfile` is detected and identified as an untracked file:
```
hg status 
# `? testfile` should be logged in PINK
```

4) Track your `testfile`:
```
hg add testfile
```

5) Check that the `testfile` is now identified as a tracked file:
```
hg status 
# `A testfile` should be logged in GREEN
```

6) Commit the tracked changes:
```
hg commit -m "Bug N/A - first test patch, please ignore r=davidwalsh"
```

7) Verify the commit by checking your commit logs:
```
hg log
# Your "Added test file" should appear at the top of the log
```

8) Submit your patch:
```
arc diff
# this should open a text file with VIM, just fill it similarly to the one below
```

```
Bug N/A - first test patch, please ignore r=davidwalsh

Summary: Test patch to see if I set MC correctly. Referring to a benign bug that's CLOSED WONTFIX

Test Plan: N/A

Reviewers: davidwalsh

Subscribers:

Bug #: 1395261
```

### Finished submitting your patch?
Double check in Phabricator that it did go through: https://phabricator.services.mozilla.com

## Common Errors

### Checking for rustc... not found
```
checking for rustc... not found
checking for cargo... not found
```

Install rust and cargo with `curl https://sh.rustup.rs -sSf | sh` and then restart your terminal. Afterwards, install cbindgen with `cargo install cbindgen`.

### Baldrdash
```
`Common error: failed to run custom build command for `baldrdash v0.1.0 (directory here)`
```
First, delete any existing `gecko-dev` folder that you might have cloned. Afterwards, delete your `mozilla-central` folder and repeat the **Cloning Mozilla Central** steps above.

If you still run into the same error, reinstall llvm with `brew uninstall llvm && brew install llvm`. Again, delete your `mozilla-central` folder and repeat the **Cloning Mozilla Central** steps above.

### Error running mach
```
 0:00.73 /usr/bin/make -f client.mk -s
 0:16.13 Error running mach:
 0:16.13     ['--log-no-times', 'artifact', 'install']
```
Delete your `mozilla-central` folder and repeat the **Cloning Mozilla Central** steps above.

### Packaging specialpowers@mozilla.org.xpi...
```
1:28.71 Packaging specialpowers@mozilla.org.xpi...
 1:29.14 Traceback (most recent call last):
 1:29.14   File "/Users/jarilvalenciano/Desktop/src/mozilla-central/config/nsinstall.py", line 188, in <module>
 1:29.14     sys.exit(_nsinstall_internal(argv[1:]))
 1:29.14   File "/Users/jarilvalenciano/Desktop/src/mozilla-central/config/nsinstall.py", line 149, in _nsinstall_internal
 1:29.14     copy_all_entries(args, target)
```
Just run `./mach build` again.