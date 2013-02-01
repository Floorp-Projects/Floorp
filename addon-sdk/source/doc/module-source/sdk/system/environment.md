<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

Module provides API to access, set and unset environment variables via exported
`env` object.

    var { env } = require('sdk/system/environment');

You can get the value of an environment variable, by accessing the
property with the name of the desired variable:

    var PATH = env.PATH;

You can check for the existence of an environment variable by checking
whether a property with that variable name exists:

    console.log('PATH' in env); // true
    console.log('FOO' in env);  // false

You can set the value of an environment variable by setting the property:

    env.FOO = 'foo';
    env.PATH += ':/my/path/'

You can unset an environment variable by deleting the property:

    delete env.FOO;

## Limitations ##

There is no way to enumerate existing environment variables, also `env`
won't have any enumerable properties:

    console.log(Object.keys(env)); // []

Environment variable will be unset, show up as non-existing if it's set
to `null`, `undefined` or `''`.

    env.FOO = null;
    console.log('FOO' in env);  // false
    env.BAR = '';
    console.log(env.BAR);       // undefined
