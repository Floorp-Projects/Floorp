[//]: # (
This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading MD5

## Introduction

We are using the md5 module to compute hashes within the source map worker.
From there we don't have easy access to privileged APIs...

## Instructions

```bash
    $ wget https://github.com/pvorb/node-md5/archive/refs/tags/v2.3.0.tar.gz
    $ tar zxvf v2.3.0.tar.gz
    $ cd node-md5-2.3.0/
```

Here edit webpack.config.js to set `libraryTarget: "umd"`,
otherwise it is packed into a script bundle instead of being kept as a commonjs.


```bash
    $ yarn webpack
    $ cp dist/md5.min.js ../md5.js # this will copy it to devtools/client/shared/vendor/md5.js
    $ cd ..
    $ rm -rf v2.3.0.tar.gz node-md5-2.3.0
```
