import * as fs from 'fs';
import * as path from 'path';
import * as process from 'process';

import { Cacheable, dataCache, setIsBuildingDataCache } from '../framework/data_cache.js';

function usage(rc: number): void {
  console.error(`Usage: tools/gen_cache [options] [OUT_DIR] [SUITE_DIRS...]

For each suite in SUITE_DIRS, pre-compute data that is expensive to generate
at runtime and store it under OUT_DIR. If the data file is found then the
DataCache will load this instead of building the expensive data at CTS runtime.

Options:
  --help          Print this message and exit.
  --list          Print the list of output files without writing them.
  --nth i/n       Only process every file where (file_index % n == i)
  --validate      Check that cache should build (Tests for collisions).
  --verbose       Print each action taken.
`);
  process.exit(rc);
}

let mode: 'emit' | 'list' | 'validate' = 'emit';
let nth = { i: 0, n: 1 };
let verbose = false;

const nonFlagsArgs: string[] = [];

for (let i = 0; i < process.argv.length; i++) {
  const arg = process.argv[i];
  if (arg.startsWith('-')) {
    switch (arg) {
      case '--list': {
        mode = 'list';
        break;
      }
      case '--help': {
        usage(0);
        break;
      }
      case '--verbose': {
        verbose = true;
        break;
      }
      case '--validate': {
        mode = 'validate';
        break;
      }
      case '--nth': {
        const err = () => {
          console.error(
            `--nth requires a value of the form 'i/n', where i and n are positive integers and i < n`
          );
          process.exit(1);
        };
        i++;
        if (i >= process.argv.length) {
          err();
        }
        const value = process.argv[i];
        const parts = value.split('/');
        if (parts.length !== 2) {
          err();
        }
        nth = { i: parseInt(parts[0]), n: parseInt(parts[1]) };
        if (nth.i < 0 || nth.n < 1 || nth.i > nth.n) {
          err();
        }
        break;
      }
      default: {
        console.log('unrecognized flag: ', arg);
        usage(1);
      }
    }
  } else {
    nonFlagsArgs.push(arg);
  }
}

if (nonFlagsArgs.length < 4) {
  usage(0);
}

const outRootDir = nonFlagsArgs[2];

dataCache.setStore({
  load: (path: string) => {
    return new Promise<Uint8Array>((resolve, reject) => {
      fs.readFile(`data/${path}`, (err, data) => {
        if (err !== null) {
          reject(err.message);
        } else {
          resolve(data);
        }
      });
    });
  },
});
setIsBuildingDataCache();

void (async () => {
  for (const suiteDir of nonFlagsArgs.slice(3)) {
    await build(suiteDir);
  }
})();

const specFileSuffix = __filename.endsWith('.ts') ? '.spec.ts' : '.spec.js';

async function crawlFilesRecursively(dir: string): Promise<string[]> {
  const subpathInfo = await Promise.all(
    (await fs.promises.readdir(dir)).map(async d => {
      const p = path.join(dir, d);
      const stats = await fs.promises.stat(p);
      return {
        path: p,
        isDirectory: stats.isDirectory(),
        isFile: stats.isFile(),
      };
    })
  );

  const files = subpathInfo
    .filter(i => i.isFile && i.path.endsWith(specFileSuffix))
    .map(i => i.path);

  return files.concat(
    await subpathInfo
      .filter(i => i.isDirectory)
      .map(i => crawlFilesRecursively(i.path))
      .reduce(async (a, b) => (await a).concat(await b), Promise.resolve([]))
  );
}

async function build(suiteDir: string) {
  if (!fs.existsSync(suiteDir)) {
    console.error(`Could not find ${suiteDir}`);
    process.exit(1);
  }

  // Crawl files and convert paths to be POSIX-style, relative to suiteDir.
  let filesToEnumerate = (await crawlFilesRecursively(suiteDir)).sort();

  // Filter out non-spec files
  filesToEnumerate = filesToEnumerate.filter(f => f.endsWith(specFileSuffix));

  const cacheablePathToTS = new Map<string, string>();

  let fileIndex = 0;
  for (const file of filesToEnumerate) {
    const pathWithoutExtension = file.substring(0, file.length - specFileSuffix.length);
    const mod = await import(`../../../${pathWithoutExtension}.spec.js`);
    if (mod.d?.serialize !== undefined) {
      const cacheable = mod.d as Cacheable<unknown>;

      {
        // Check for collisions
        const existing = cacheablePathToTS.get(cacheable.path);
        if (existing !== undefined) {
          console.error(
            `error: Cacheable '${cacheable.path}' is emitted by both:
    '${existing}'
and
    '${file}'`
          );
          process.exit(1);
        }
        cacheablePathToTS.set(cacheable.path, file);
      }

      const outPath = `${outRootDir}/data/${cacheable.path}`;

      if (fileIndex++ % nth.n === nth.i) {
        switch (mode) {
          case 'emit': {
            if (verbose) {
              console.log(`building '${outPath}'`);
            }
            const data = await cacheable.build();
            const serialized = cacheable.serialize(data);
            fs.mkdirSync(path.dirname(outPath), { recursive: true });
            fs.writeFileSync(outPath, serialized, 'binary');
            break;
          }
          case 'list': {
            console.log(outPath);
            break;
          }
          case 'validate': {
            // Only check currently performed is the collision detection above
            break;
          }
        }
      }
    }
  }
}
