.. _test_certificates:

===============================
Adding Certificates for Testing
===============================

Sometimes we need to write tests for scenarios that require custom client, server or certificate authority (CA) certificates. For that purpose, you can generate such certificates using ``build/pgo/genpgocert.py``.

The certificate specifications (and key specifications) are located in ``build/pgo/certs/``.

To add a new **server certificate**, add a ``${cert_name}.certspec`` file to that folder.
If it needs a non-default private key, add a corresponding ``${cert_name}.server.keyspec``.

For a new **client certificate**, add a ``${cert_name}.client.keyspec`` and corresponding ``${cert_name}.certspec``.

To add a new **CA**, add a ``${cert_name}.ca.keyspec`` as well as a corresponding ``${cert_name}.certspec`` to that folder.

.. hint::

   * The full syntax for .certspec files is documented at https://searchfox.org/mozilla-central/source/security/manager/tools/pycert.py

   * The full syntax for .keyspec files is documented at https://searchfox.org/mozilla-central/source/security/manager/tools/pykey.py

Then regenerate the certificates by running:::

   ./mach python build/pgo/genpgocert.py

These commands will modify cert9.db and key4.db, and if you have added a .keyspec file will generate a ``{$cert_name}.client`` or ``{$cert_name}.ca`` file.

**These files need to be committed.**

If you've created a new server certificate, you probably want to modify ``build/pgo/server-locations.txt`` to add a location with your specified certificate:::

   https://my-test.example.com:443           cert=${cert_name}

You will need to run ``./mach build`` again afterwards.

.. important::

   Make sure to exactly follow the naming conventions and use the same ``cert_name`` in all places
