import * as process from 'process';

import { crawl } from './crawl.js';

function usage(rc: number): void {
  console.error(`Usage: tools/validate [options] [SUITE_DIRS...]

For each suite in SUITE_DIRS, validate some properties about the file:
- It has a .description and .g
- That each test:
  - Has a test function (or is marked unimplemented)
  - Has no duplicate cases
  - Configures batching correctly, if used

Example:
  tools/validate src/unittests/ src/webgpu/

Options:
  --help          Print this message and exit.
`);
  process.exit(rc);
}

const args = process.argv.slice(2);
if (args.indexOf('--help') !== -1) {
  usage(0);
}

if (args.length < 1) {
  usage(0);
}

for (const suiteDir of args) {
  void crawl(suiteDir, true);
}
