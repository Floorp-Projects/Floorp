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
        cmd: 'node',
        args: ['tools/gen_listings', 'out/', 'src/webgpu', 'src/stress', 'src/manual', 'src/unittests', 'src/demo'],
      },
      'generate-wpt-cts-html': {
        cmd: 'node',
        args: ['tools/gen_wpt_cts_html', 'out-wpt/cts.https.html', 'src/common/templates/cts.https.html'],
      },
      'generate-cache': {
        cmd: 'node',
        args: ['tools/gen_cache', 'out/data', 'src/webgpu'],
      },
      unittest: {
        cmd: 'node',
        args: ['tools/run_node', 'unittests:*'],
      },
      'build-out': {
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
          '--only=src/common/framework/',
          '--only=src/common/runtime/helper/',
          '--only=src/common/runtime/wpt.ts',
          '--only=src/webgpu/',
          // These files will be generated, instead of compiled from TypeScript.
          '--ignore=src/common/internal/version.ts',
          '--ignore=src/webgpu/listing.ts',
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

  grunt.registerTask('set-quiet-mode', () => {
    grunt.log.write('Running tasks');
    require('quiet-grunt');
  });

  grunt.registerTask('build-standalone', 'Build out/ (no checks, no WPT)', [
    'run:build-out',
    'run:copy-assets',
    'run:generate-version',
    'run:generate-listings',
  ]);
  grunt.registerTask('build-wpt', 'Build out/ (no checks)', [
    'run:build-out-wpt',
    'run:copy-assets-wpt',
    'run:autoformat-out-wpt',
    'run:generate-version',
    'run:generate-listings',
    'copy:out-wpt-generated',
    'copy:out-wpt-htmlfiles',
    'run:generate-wpt-cts-html',
  ]);
  grunt.registerTask('build-done-message', () => {
    process.stderr.write('\nBuild completed! Running checks/tests');
  });

  registerTaskAndAddToHelp('pre', 'Run all presubmit checks: standalone+wpt+typecheck+unittest+lint', [
    'set-quiet-mode',
    'clean',
    'build-standalone',
    'build-wpt',
    'run:build-out-node',
    'build-done-message',
    'ts:check',
    'run:presubmit',
    'run:unittest',
    'run:lint',
    'run:tsdoc-treatWarningsAsErrors',
  ]);
  registerTaskAndAddToHelp('standalone', 'Build standalone and typecheck', [
    'set-quiet-mode',
    'build-standalone',
    'build-done-message',
    'ts:check',
  ]);
  registerTaskAndAddToHelp('wpt', 'Build for WPT and typecheck', [
    'set-quiet-mode',
    'build-wpt',
    'build-done-message',
    'ts:check',
  ]);
  registerTaskAndAddToHelp('unittest', 'Build standalone, typecheck, and unittest', [
    'standalone',
    'run:unittest',
  ]);
  registerTaskAndAddToHelp('check', 'Just typecheck', [
    'set-quiet-mode',
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
