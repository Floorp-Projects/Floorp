Loop Client
===========

Prerequisites
-------------

NodeJS and npm installed.

Installation
------------

Fetch and install/build any NPM dependencies:

    $ make install

Configuration
-------------

If you need a static config.js file for deployment (most people wont; only
folks deploying the development server will!), you can generate one like this:

    $ make config

It will read the configuration from the following env variables and generate the
appropriate configuration file:

- `LOOP_SERVER_URL` defines the root url of the loop server, without trailing
  slash (default: `http://localhost:5000`).
- `LOOP_FEEDBACK_API_URL` sets the root URL for the
  [input API](https://input.mozilla.org/); defaults to the input stage server
  (https://input.allizom.org/api/v1/feedback). **Don't forget to set this
  value to the production server URL when deploying to production.**
- `LOOP_FEEDBACK_PRODUCT_NAME` defines the product name to be sent to the input
  API (defaults: Loop).

Usage
-----

For development, run a local static file server:

    $ make runserver

Then point your browser at:

- `http://localhost:3000/content/` for all public webapp contents,
- `http://localhost:3000/test/` for tests.

**Note:** the provided static file server for web contents is **not** intended
for production use.

License
-------

The Loop server code is released under the terms of the
[Mozilla Public License v2.0](http://www.mozilla.org/MPL/2.0/). See the
`LICENSE` file at the root of the repository.
