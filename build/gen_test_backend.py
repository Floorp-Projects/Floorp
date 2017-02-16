# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from mozbuild.backend.test_manifest import TestManifestBackend
from mozbuild.base import BuildEnvironmentNotFoundException, MozbuildObject
from mozbuild.frontend.emitter import TreeMetadataEmitter
from mozbuild.frontend.reader import BuildReader, EmptyConfig


def gen_test_backend():
    build_obj = MozbuildObject.from_environment()
    try:
        config = build_obj.config_environment
    except BuildEnvironmentNotFoundException:
        print("No build detected, test metadata may be incomplete.")
        config = EmptyConfig(build_obj.topsrcdir)
        config.topobjdir = build_obj.topobjdir

    reader = BuildReader(config)
    emitter = TreeMetadataEmitter(config)
    backend = TestManifestBackend(config)

    context = reader.read_topsrcdir()
    data = emitter.emit(context, emitfn=emitter._process_test_manifests)
    backend.consume(data)


if __name__ == '__main__':
    sys.exit(gen_test_backend())
