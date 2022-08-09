yamllint
========

`yamllint <https://github.com/adrienverge/yamllint>`__ is a linter for YAML files.


Run Locally
-----------

The mozlint integration of yamllint can be run using mach:

.. parsed-literal::

    $ mach lint --linter yaml <file paths>

Alternatively, omit ``--linter yaml`` to run all configured linters, including
yamllint.


Configuration
-------------

To enable yamllint on a new directory, add the path to the include section in
the :searchfox:`yaml.yml <tools/lint/yaml.yml>` file.


Sources
-------

* :searchfox:`Configuration (YAML) <tools/lint/yaml.yml>`
* :searchfox:`Source <tools/lint/yamllint_/__init__.py>`
