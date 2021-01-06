# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# adaptor.py -- Use generators to mutate the sequence of tests to be executed.


def xdr_annotate(tests, options):
    """Returns a tuple a test which is encoding the self-hosted
    code and a generator of tests which is decoding the self-hosted
    code."""
    selfhosted_is_encoded = False
    for test in tests:
        if not test.enable and not options.run_skipped:
            test.selfhosted_xdr_mode = "off"
        elif not selfhosted_is_encoded:
            test.selfhosted_xdr_mode = "encode"
            selfhosted_is_encoded = True
        else:
            test.selfhosted_xdr_mode = "decode"
        yield test
