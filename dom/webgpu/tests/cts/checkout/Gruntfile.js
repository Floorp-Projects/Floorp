/* eslint-disable node/no-unpublished-require */
/* eslint-disable prettier/prettier */
/* eslint-disable no-console */

module.exports = function (grunt) {
  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON('package.json'),

    clean: {
      out: ['out/', 'out-wpt/', 'out-node/'],
    },

    run: {
      'generate-version': {
        cmd: 'node',
        args: ['tools/gen_version'],
      },
      'generate-listings': {
        // Overwrites the listings.js files in out/. Must run before copy:out-wpt-generated;
        // must not run before run:build-out (if it is run).
        cmd: 'node',
        args: ['tools/gen_listings', 'out/', 'src/webgpu', 'src/stress', 'src/manual', 'src/unittests', 'src/demo'],
      },
      validate: {
        cmd: 'node',
        args: ['tools/validate', 'src/webgpu', 'src/stress', 'src/manual', 'src/unittests', 'src/demo'],
      },
      'generate-wpt-cts-html': {
        cmd: 'node',
        args: ['tools/gen_wpt_cts_html', 'tools/gen_wpt_cfg_unchunked.json'],
      },
      'generate-wpt-cts-html-chunked2sec': {
        cmd: 'node',
        args: ['tools/gen_wpt_cts_html', 'tools/gen_wpt_cfg_chunked2sec.json'],
      },
      'generate-cache': {
        cmd: 'node',
        args: ['tools/gen_cache', 'out', 'src/webgpu'],
      },
      unittest: {
        cmd: 'node',
        args: ['tools/run_node', 'unittests:*'],
      },
      'build-out': {
        // Must run before run:generate-listings, which will overwrite some files.
        cmd: 'node',
        args: [
          'node_modules/@babel/cli/bin/babel',
          '--extensions=.ts,.js',
          '--source-maps=true',
          '--out-dir=out/',
          'src/',
        ],
      },
      'build-out-wpt': {
        cmd: 'node',
        args: [
          'node_modules/@babel/cli/bin/babel',
          '--extensions=.ts,.js',
          '--source-maps=false',
          '--delete-dir-on-start',
          '--out-dir=out-wpt/',
          'src/',
          '--only=src/common/',
          '--only=src/external/',
          '--only=src/webgpu/',
          // These files will be generated, instead of compiled from TypeScript.
          '--ignore=src/common/internal/version.ts',
          '--ignore=src/webgpu/listing.ts',
          // These files are only used by non-WPT builds.
          '--ignore=src/common/runtime/cmdline.ts',
          '--ignore=src/common/runtime/server.ts',
          '--ignore=src/common/runtime/standalone.ts',
          '--ignore=src/common/runtime/helper/sys.ts',
          '--ignore=src/common/tools',
        ],
      },
      'build-out-node': {
        cmd: 'node',
        args: [
          'node_modules/typescript/lib/tsc.js',
          '--project', 'node.tsconfig.json',
          '--outDir', 'out-node/',
        ],
      },
      'copy-assets': {
        cmd: 'node',
        args: [
          'node_modules/@babel/cli/bin/babel',
          'src/resources/',
          '--out-dir=out/resources/',
          '--copy-files'
        ],
      },
      'copy-assets-wpt': {
        cmd: 'node',
        args: [
          'node_modules/@babel/cli/bin/babel',
          'src/resources/',
          '--out-dir=out-wpt/resources/',
          '--copy-files'
        ],
      },
      lint: {
        cmd: 'node',
        args: ['node_modules/eslint/bin/eslint', 'src/**/*.ts', '--max-warnings=0'],
      },
      presubmit: {
        cmd: 'node',
        args: ['tools/presubmit'],
      },
      fix: {
        cmd: 'node',
        args: ['node_modules/eslint/bin/eslint', 'src/**/*.ts', '--fix'],
      },
      'autoformat-out-wpt': {
        cmd: 'node',
        args: ['node_modules/prettier/bin-prettier', '--loglevel=warn', '--write', 'out-wpt/**/*.js'],
      },
      tsdoc: {
        cmd: 'node',
        args: ['node_modules/typedoc/bin/typedoc'],
      },
      'tsdoc-treatWarningsAsErrors': {
        cmd: 'node',
        args: ['node_modules/typedoc/bin/typedoc', '--treatWarningsAsErrors'],
      },

      serve: {
        cmd: 'node',
        args: ['node_modules/http-server/bin/http-server', '-p8080', '-a127.0.0.1', '-c-1']
      }
    },

    copy: {
      'out-wpt-generated': {
        files: [
          // Must run after run:generate-version and run:generate-listings.
          { expand: true, cwd: 'out', src: 'common/internal/version.js', dest: 'out-wpt/' },
          { expand: true, cwd: 'out', src: 'webgpu/listing.js', dest: 'out-wpt/' },
        ],
      },
      'out-wpt-htmlfiles': {
        files: [
          { expand: true, cwd: 'src', src: 'webgpu/**/*.html', dest: 'out-wpt/' },
        ],
      },
    },

    ts: {
      check: {
        tsconfig: {
          tsconfig: 'tsconfig.json',
          passThrough: true,
        },
      },
    },
  });

  grunt.loadNpmTasks('grunt-contrib-clean');
  grunt.loadNpmTasks('grunt-contrib-copy');
  grunt.loadNpmTasks('grunt-run');
  grunt.loadNpmTasks('grunt-ts');

  const helpMessageTasks = [];
  function registerTaskAndAddToHelp(name, desc, deps) {
    grunt.registerTask(name, deps);
    addExistingTaskToHelp(name, desc);
  }
  function addExistingTaskToHelp(name, desc) {
    helpMessageTasks.push({ name, desc });
  }

  grunt.registerTask('build-standalone', 'Build out/ (no listings, no checks, no WPT)', [
    'run:build-out',
    'run:copy-assets',
    'run:generate-version',
  ]);
  grunt.registerTask('build-wpt', 'Build out-wpt/ (no checks; run after generate-listings)', [
    'run:build-out-wpt',
    'run:copy-assets-wpt',
    'run:autoformat-out-wpt',
    'run:generate-version',
    'copy:out-wpt-generated',
    'copy:out-wpt-htmlfiles',
    'run:generate-wpt-cts-html',
    'run:generate-wpt-cts-html-chunked2sec',
  ]);
  grunt.registerTask('build-done-message', () => {
    process.stderr.write('\nBuild completed! Running checks/tests');
  });

  registerTaskAndAddToHelp('pre', 'Run all presubmit checks: standalone+wpt+typecheck+unittest+lint', [
    'clean',
    'run:validate',
    'build-standalone',
    'run:generate-listings',
    'build-wpt',
    'run:build-out-node',
    'run:generate-cache',
    'build-done-message',
    'ts:check',
    'run:presubmit',
    'run:unittest',
    'run:lint',
    'run:tsdoc-treatWarningsAsErrors',
  ]);
  registerTaskAndAddToHelp('standalone', 'Build standalone and typecheck', [
    'build-standalone',
    'run:generate-listings',
    'build-done-message',
    'run:validate',
    'ts:check',
  ]);
  registerTaskAndAddToHelp('wpt', 'Build for WPT and typecheck', [
    'run:generate-listings',
    'build-wpt',
    'build-done-message',
    'run:validate',
    'ts:check',
  ]);
  registerTaskAndAddToHelp('unittest', 'Build standalone, typecheck, and unittest', [
    'standalone',
    'run:unittest',
  ]);
  registerTaskAndAddToHelp('check', 'Just typecheck', [
    'ts:check',
  ]);

  registerTaskAndAddToHelp('serve', 'Serve out/ on 127.0.0.1:8080 (does NOT compile source)', ['run:serve']);
  registerTaskAndAddToHelp('fix', 'Fix lint and formatting', ['run:fix']);

  addExistingTaskToHelp('clean', 'Clean out/ and out-wpt/');

  grunt.registerTask('default', '', () => {
    console.error('\nAvailable tasks (see grunt --help for info):');
    for (const { name, desc } of helpMessageTasks) {
      console.error(`$ grunt ${name}`);
      console.error(`  ${desc}`);
    }
  });
};
