# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This module contains code for managing WebIDL files and bindings for
# the build system.

from __future__ import unicode_literals

import errno
import hashlib
import json
import logging
import os

from copy import deepcopy

from mach.mixin.logging import LoggingMixin

from mozbuild.base import MozbuildObject
from mozbuild.makeutil import Makefile
from mozbuild.pythonutil import iter_modules_in_path
from mozbuild.util import FileAvoidWrite

import mozpack.path as mozpath

# There are various imports in this file in functions to avoid adding
# dependencies to config.status. See bug 949875.


class BuildResult(object):
    """Represents the result of processing WebIDL files.

    This holds a summary of output file generation during code generation.
    """

    def __init__(self):
        # The .webidl files that had their outputs regenerated.
        self.inputs = set()

        # The output files that were created.
        self.created = set()

        # The output files that changed.
        self.updated = set()

        # The output files that didn't change.
        self.unchanged = set()


class WebIDLCodegenManagerState(dict):
    """Holds state for the WebIDL code generation manager.

    State is currently just an extended dict. The internal implementation of
    state should be considered a black box to everyone except
    WebIDLCodegenManager. But we'll still document it.

    Fields:

    version
       The integer version of the format. This is to detect incompatible
       changes between state. It should be bumped whenever the format
       changes or semantics change.

    webidls
       A dictionary holding information about every known WebIDL input.
       Keys are the basenames of input WebIDL files. Values are dicts of
       metadata. Keys in those dicts are:

       * filename - The full path to the input filename.
       * inputs - A set of full paths to other webidl files this webidl
         depends on.
       * outputs - Set of full output paths that are created/derived from
         this file.
       * sha1 - The hexidecimal SHA-1 of the input filename from the last
         processing time.

    global_inputs
       A dictionary defining files that influence all processing. Keys
       are full filenames. Values are hexidecimal SHA-1 from the last
       processing time.
    """

    VERSION = 1

    def __init__(self, fh=None):
        self['version'] = self.VERSION
        self['webidls'] = {}
        self['global_depends'] = {}

        if not fh:
            return

        state = json.load(fh)
        if state['version'] != self.VERSION:
            raise Exception('Unknown state version: %s' % state['version'])

        self['version'] = state['version']
        self['global_depends'] = state['global_depends']

        for k, v in state['webidls'].items():
            self['webidls'][k] = v

            # Sets are converted to lists for serialization because JSON
            # doesn't support sets.
            self['webidls'][k]['inputs'] = set(v['inputs'])
            self['webidls'][k]['outputs'] = set(v['outputs'])

    def dump(self, fh):
        """Dump serialized state to a file handle."""
        normalized = deepcopy(self)

        for k, v in self['webidls'].items():
            # Convert sets to lists because JSON doesn't support sets.
            normalized['webidls'][k]['outputs'] = sorted(v['outputs'])
            normalized['webidls'][k]['inputs'] = sorted(v['inputs'])

        json.dump(normalized, fh, sort_keys=True)


class WebIDLCodegenManager(LoggingMixin):
    """Manages all code generation around WebIDL.

    To facilitate testing, this object is meant to be generic and reusable.
    Paths, etc should be parameters and not hardcoded.
    """

    # Global parser derived declaration files.
    GLOBAL_DECLARE_FILES = {
        'FeatureList.h',
        'GeneratedAtomList.h',
        'PrototypeList.h',
        'RegisterBindings.h',
        'UnionConversions.h',
        'UnionTypes.h',
    }

    # Global parser derived definition files.
    GLOBAL_DEFINE_FILES = {
        'RegisterBindings.cpp',
        'UnionTypes.cpp',
        'PrototypeList.cpp',
    }

    def __init__(self, config_path, inputs, exported_header_dir,
        codegen_dir, state_path, cache_dir=None, make_deps_path=None,
        make_deps_target=None):
        """Create an instance that manages WebIDLs in the build system.

        config_path refers to a WebIDL config file (e.g. Bindings.conf).
        inputs is a 4-tuple describing the input .webidl files and how to
        process them. Members are:
            (set(.webidl files), set(basenames of exported files),
                set(basenames of generated events files),
                set(example interface names))

        exported_header_dir and codegen_dir are directories where generated
        files will be written to.
        state_path is the path to a file that will receive JSON state from our
        actions.
        make_deps_path is the path to a make dependency file that we can
        optionally write.
        make_deps_target is the target that receives the make dependencies. It
        must be defined if using make_deps_path.
        """
        self.populate_logger()

        input_paths, exported_stems, generated_events_stems, example_interfaces = inputs

        self._config_path = config_path
        self._input_paths = set(input_paths)
        self._exported_stems = set(exported_stems)
        self._generated_events_stems = set(generated_events_stems)
        self._example_interfaces = set(example_interfaces)
        self._exported_header_dir = exported_header_dir
        self._codegen_dir = codegen_dir
        self._state_path = state_path
        self._cache_dir = cache_dir
        self._make_deps_path = make_deps_path
        self._make_deps_target = make_deps_target

        if (make_deps_path and not make_deps_target) or (not make_deps_path and
            make_deps_target):
            raise Exception('Must define both make_deps_path and make_deps_target '
                'if one is defined.')

        self._parser_results = None
        self._config = None
        self._state = WebIDLCodegenManagerState()

        if os.path.exists(state_path):
            with open(state_path, 'rb') as fh:
                try:
                    self._state = WebIDLCodegenManagerState(fh=fh)
                except Exception as e:
                    self.log(logging.WARN, 'webidl_bad_state', {'msg': str(e)},
                        'Bad WebIDL state: {msg}')

    @property
    def config(self):
        if not self._config:
            self._parse_webidl()

        return self._config

    def generate_build_files(self):
        """Generate files required for the build.

        This function is in charge of generating all the .h/.cpp files derived
        from input .webidl files. Please note that there are build actions
        required to produce .webidl files and these build actions are
        explicitly not captured here: this function assumes all .webidl files
        are present and up to date.

        This routine is called as part of the build to ensure files that need
        to exist are present and up to date. This routine may not be called if
        the build dependencies (generated as a result of calling this the first
        time) say everything is up to date.

        Because reprocessing outputs for every .webidl on every invocation
        is expensive, we only regenerate the minimal set of files on every
        invocation. The rules for deciding what needs done are roughly as
        follows:

        1. If any .webidl changes, reparse all .webidl files and regenerate
           the global derived files. Only regenerate output files (.h/.cpp)
           impacted by the modified .webidl files.
        2. If an non-.webidl dependency (Python files, config file) changes,
           assume everything is out of date and regenerate the world. This
           is because changes in those could globally impact every output
           file.
        3. If an output file is missing, ensure it is present by performing
           necessary regeneration.
        """
        # Despite #1 above, we assume the build system is smart enough to not
        # invoke us if nothing has changed. Therefore, any invocation means
        # something has changed. And, if anything has changed, we need to
        # parse the WebIDL.
        self._parse_webidl()

        result = BuildResult()

        # If we parse, we always update globals - they are cheap and it is
        # easier that way.
        created, updated, unchanged = self._write_global_derived()
        result.created |= created
        result.updated |= updated
        result.unchanged |= unchanged

        # If any of the extra dependencies changed, regenerate the world.
        global_changed, global_hashes = self._global_dependencies_changed()
        if global_changed:
            # Make a copy because we may modify.
            changed_inputs = set(self._input_paths)
        else:
            changed_inputs = self._compute_changed_inputs()

        self._state['global_depends'] = global_hashes

        # Generate bindings from .webidl files.
        for filename in sorted(changed_inputs):
            basename = mozpath.basename(filename)
            result.inputs.add(filename)
            written, deps = self._generate_build_files_for_webidl(filename)
            result.created |= written[0]
            result.updated |= written[1]
            result.unchanged |= written[2]

            self._state['webidls'][basename] = dict(
                filename=filename,
                outputs=written[0] | written[1] | written[2],
                inputs=set(deps),
                sha1=self._input_hashes[filename],
            )

        # Process some special interfaces required for testing.
        for interface in self._example_interfaces:
            written = self.generate_example_files(interface)
            result.created |= written[0]
            result.updated |= written[1]
            result.unchanged |= written[2]

        # Generate a make dependency file.
        if self._make_deps_path:
            mk = Makefile()
            codegen_rule = mk.create_rule([self._make_deps_target])
            codegen_rule.add_dependencies(global_hashes.keys())
            codegen_rule.add_dependencies(self._input_paths)

            with FileAvoidWrite(self._make_deps_path) as fh:
                mk.dump(fh)

        self._save_state()

        return result

    def generate_example_files(self, interface):
        """Generates example files for a given interface."""
        from Codegen import CGExampleRoot

        root = CGExampleRoot(self.config, interface)

        return self._maybe_write_codegen(root, *self._example_paths(interface))

    def _parse_webidl(self):
        import WebIDL
        from Configuration import Configuration

        self.log(logging.INFO, 'webidl_parse',
            {'count': len(self._input_paths)},
            'Parsing {count} WebIDL files.')

        hashes = {}
        parser = WebIDL.Parser(self._cache_dir)

        for path in sorted(self._input_paths):
            with open(path, 'rb') as fh:
                data = fh.read()
                hashes[path] = hashlib.sha1(data).hexdigest()
                parser.parse(data, path)

        self._parser_results = parser.finish()
        self._config = Configuration(self._config_path, self._parser_results)
        self._input_hashes = hashes

    def _write_global_derived(self):
        from Codegen import GlobalGenRoots

        things = [('declare', f) for f in self.GLOBAL_DECLARE_FILES]
        things.extend(('define', f) for f in self.GLOBAL_DEFINE_FILES)

        result = (set(), set(), set())

        for what, filename in things:
            stem = mozpath.splitext(filename)[0]
            root = getattr(GlobalGenRoots, stem)(self._config)

            if what == 'declare':
                code = root.declare()
                output_root = self._exported_header_dir
            elif what == 'define':
                code = root.define()
                output_root = self._codegen_dir
            else:
                raise Exception('Unknown global gen type: %s' % what)

            output_path = mozpath.join(output_root, filename)
            self._maybe_write_file(output_path, code, result)

        return result

    def _compute_changed_inputs(self):
        """Compute the set of input files that need to be regenerated."""
        changed_inputs = set()
        expected_outputs = self.expected_build_output_files()

        # Look for missing output files.
        if any(not os.path.exists(f) for f in expected_outputs):
            # FUTURE Bug 940469 Only regenerate minimum set.
            changed_inputs |= self._input_paths

        # That's it for examining output files. We /could/ examine SHA-1's of
        # output files from a previous run to detect modifications. But that's
        # a lot of extra work and most build systems don't do that anyway.

        # Now we move on to the input files.
        old_hashes = {v['filename']: v['sha1']
            for v in self._state['webidls'].values()}

        old_filenames = set(old_hashes.keys())
        new_filenames = self._input_paths

        # If an old file has disappeared or a new file has arrived, mark
        # it.
        changed_inputs |= old_filenames ^ new_filenames

        # For the files in common between runs, compare content. If the file
        # has changed, mark it. We don't need to perform mtime comparisons
        # because content is a stronger validator.
        for filename in old_filenames & new_filenames:
            if old_hashes[filename] != self._input_hashes[filename]:
                changed_inputs.add(filename)

        # We've now populated the base set of inputs that have changed.

        # Inherit dependencies from previous run. The full set of dependencies
        # is associated with each record, so we don't need to perform any fancy
        # graph traversal.
        for v in self._state['webidls'].values():
            if any(dep for dep in v['inputs'] if dep in changed_inputs):
                changed_inputs.add(v['filename'])

        # Only use paths that are known to our current state.
        # This filters out files that were deleted or changed type (e.g. from
        # static to preprocessed).
        return changed_inputs & self._input_paths

    def _binding_info(self, p):
        """Compute binding metadata for an input path.

        Returns a tuple of:

          (stem, binding_stem, is_event, output_files)

        output_files is itself a tuple. The first two items are the binding
        header and C++ paths, respectively. The 2nd pair are the event header
        and C++ paths or None if this isn't an event binding.
        """
        basename = mozpath.basename(p)
        stem = mozpath.splitext(basename)[0]
        binding_stem = '%sBinding' % stem

        if stem in self._exported_stems:
            header_dir = self._exported_header_dir
        else:
            header_dir = self._codegen_dir

        is_event = stem in self._generated_events_stems

        files = (
            mozpath.join(header_dir, '%s.h' % binding_stem),
            mozpath.join(self._codegen_dir, '%s.cpp' % binding_stem),
            mozpath.join(header_dir, '%s.h' % stem) if is_event else None,
            mozpath.join(self._codegen_dir, '%s.cpp' % stem) if is_event else None,
        )

        return stem, binding_stem, is_event, header_dir, files

    def _example_paths(self, interface):
        return (
            mozpath.join(self._codegen_dir, '%s-example.h' % interface),
            mozpath.join(self._codegen_dir, '%s-example.cpp' % interface))

    def expected_build_output_files(self):
        """Obtain the set of files generate_build_files() should write."""
        paths = set()

        # Account for global generation.
        for p in self.GLOBAL_DECLARE_FILES:
            paths.add(mozpath.join(self._exported_header_dir, p))
        for p in self.GLOBAL_DEFINE_FILES:
            paths.add(mozpath.join(self._codegen_dir, p))

        for p in self._input_paths:
            stem, binding_stem, is_event, header_dir, files = self._binding_info(p)
            paths |= {f for f in files if f}

        for interface in self._example_interfaces:
            for p in self._example_paths(interface):
                paths.add(p)

        return paths

    def _generate_build_files_for_webidl(self, filename):
        from Codegen import (
            CGBindingRoot,
            CGEventRoot,
        )

        self.log(logging.INFO, 'webidl_generate_build_for_input',
            {'filename': filename},
            'Generating WebIDL files derived from {filename}')

        stem, binding_stem, is_event, header_dir, files = self._binding_info(filename)
        root = CGBindingRoot(self._config, binding_stem, filename)

        result = self._maybe_write_codegen(root, files[0], files[1])

        if is_event:
            generated_event = CGEventRoot(self._config, stem)
            result = self._maybe_write_codegen(generated_event, files[2],
                files[3], result)

        return result, root.deps()

    def _global_dependencies_changed(self):
        """Determine whether the global dependencies have changed."""
        current_files = set(iter_modules_in_path(mozpath.dirname(__file__)))

        # We need to catch other .py files from /dom/bindings. We assume these
        # are in the same directory as the config file.
        current_files |= set(iter_modules_in_path(mozpath.dirname(self._config_path)))

        current_files.add(self._config_path)

        current_hashes = {}
        for f in current_files:
            # This will fail if the file doesn't exist. If a current global
            # dependency doesn't exist, something else is wrong.
            with open(f, 'rb') as fh:
                current_hashes[f] = hashlib.sha1(fh.read()).hexdigest()

        # The set of files has changed.
        if current_files ^ set(self._state['global_depends'].keys()):
            return True, current_hashes

        # Compare hashes.
        for f, sha1 in current_hashes.items():
            if sha1 != self._state['global_depends'][f]:
                return True, current_hashes

        return False, current_hashes

    def _save_state(self):
        with open(self._state_path, 'wb') as fh:
            self._state.dump(fh)

    def _maybe_write_codegen(self, obj, declare_path, define_path, result=None):
        assert declare_path and define_path
        if not result:
            result = (set(), set(), set())

        self._maybe_write_file(declare_path, obj.declare(), result)
        self._maybe_write_file(define_path, obj.define(), result)

        return result

    def _maybe_write_file(self, path, content, result):
        fh = FileAvoidWrite(path)
        fh.write(content)
        existed, updated = fh.close()

        if not existed:
            result[0].add(path)
        elif updated:
            result[1].add(path)
        else:
            result[2].add(path)


def create_build_system_manager(topsrcdir, topobjdir, dist_dir):
    """Create a WebIDLCodegenManager for use by the build system."""
    src_dir = os.path.join(topsrcdir, 'dom', 'bindings')
    obj_dir = os.path.join(topobjdir, 'dom', 'bindings')

    with open(os.path.join(obj_dir, 'file-lists.json'), 'rb') as fh:
        files = json.load(fh)

    inputs = (files['webidls'], files['exported_stems'],
        files['generated_events_stems'], files['example_interfaces'])

    cache_dir = os.path.join(obj_dir, '_cache')
    try:
        os.makedirs(cache_dir)
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

    return WebIDLCodegenManager(
        os.path.join(src_dir, 'Bindings.conf'),
        inputs,
        os.path.join(dist_dir, 'include', 'mozilla', 'dom'),
        obj_dir,
        os.path.join(obj_dir, 'codegen.json'),
        cache_dir=cache_dir,
        # The make rules include a codegen.pp file containing dependencies.
        make_deps_path=os.path.join(obj_dir, 'codegen.pp'),
        make_deps_target='codegen.pp',
    )


class BuildSystemWebIDL(MozbuildObject):
    @property
    def manager(self):
        if not hasattr(self, '_webidl_manager'):
            self._webidl_manager = create_build_system_manager(
                self.topsrcdir, self.topobjdir, self.distdir)

        return self._webidl_manager
