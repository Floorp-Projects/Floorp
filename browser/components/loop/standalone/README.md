Loop Client
===========

Prerequisites
-------------

NodeJS and npm installed.

Installation
------------

    $ make install

Configuration
-------------

You will need to generate a configuration file, you can do so with:

	$ make config

It will read the configuration from the `LOOP_SERVER_URL` env variable and
generate the appropriate configuration file. This setting defines the root url
of the loop server, without trailing slash.

Usage
-----

For development, run a local static file server:

    $ make runserver

Then point your browser at:

- `http://localhost:3000/content/` for all public webapp contents,
- `http://localhost:3000/test/` for tests.

**Note:** the provided static file server for web contents is **not** intended
for production use.

Code linting
------------

    $ make lint

License
-------

The Loop server code is released under the terms of the
[Mozilla Public License v2.0](http://www.mozilla.org/MPL/2.0/). See the
`LICENSE` file at the root of the repository.
