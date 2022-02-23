# Setup a Virtual Machine running OpenBSD to generate OpenBSD build files for libwebrtc.

1. Download the OpenBSD 7.0 ISO [here](https://cdn.openbsd.org/pub/OpenBSD/7.0/amd64/install70.iso)

2. Start by setting up a new VM. These steps reference VMWare fusion options,
   but they should translate to other VM hosts.  During the VM creation steps,
   using the following settings:
   - VMWare - Other Linux 5.x kernel 64-bit
   - VMWare - Legacy BIOS
   - VMWare - Customize settings
   - VMWare - Processors & Memory - 2 cores, 2048MB Memory
   - VMWare - Hard Disk - 50.00 GB

3. Start VM to begin the OpenBSD install process.

   In general, choose the default options for everything unless listed below:
   - select ```(I)nstall```
   - allow root ssh - ```yes```
   - disk - for more info see [here](https://azcrumpty.weebly.com/journal/easy-openbsd-partition-scheme)
       - ```c``` # for custom
       - ```z``` # for delete all
       - ```a a``` # for add 'a' partition
       - enter forward slash ('/') for the mount point
       - ```q``` # to quit editor
   - directory does not contain SHA256.sig. Continue without verification? ```yes```

4. Upgrade to OpenBSD -current

   Upgrading to -current is required for the minimum rust version.  For more
   info, see [this](https://unix.stackexchange.com/questions/406870/how-to-follow-openbsd-current).

   - login as root

           ftp -o /bsd.rd https://cloudflare.cdn.openbsd.org/pub/OpenBSD/snapshots/amd64/bsd.rd
           reboot

   - at the bootloader prompt _quickly_ type

           boot bsd.rd

   - select ```U(Upgrade)``` at first prompt question, default to all other questions
   - after upgrade completes allow reboot

5. Upgrade and install necessary packages, add non-root user

   - login as root

         pkg_add -u
         pkg_add sudo-- git-- bash-- mercurial-- m4-- py3-pip--
         ln -sf /usr/local/bin/pip3.9 /usr/local/bin/pip3
         ln -sf /usr/local/bin/python3 /usr/local/bin/python
         visudo # uncomment line with '%wheel        ALL=(ALL) NOPASSWD: SETENV: ALL'

         export NEW_USER={pick-a-user-name}
         useradd -b /home -m -G wheel $NEW_USER
         usermod -s /usr/local/bin/bash $NEW_USER
         passwd $NEW_USER

   - logout

6. Clone mozilla-central repository and bootstrap

   - login as {NEW_USER}

           hg clone https://hg.mozilla.org/mozilla-central
           (cd mozilla-central && ./mach bootstrap)

   - for bootstrap:
       - ignoring errors around glean-sdk
       - select option ```1``` for all pkg_add prompts


7. Continue with step 4 in README.md
