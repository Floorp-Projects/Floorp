Loop Client
===========

Prerequisites
-------------

NodeJS and npm installed.

Installation
------------

    $ make install

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
