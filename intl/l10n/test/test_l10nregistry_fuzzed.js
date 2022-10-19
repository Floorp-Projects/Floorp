/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test is a fuzzing test for the L10nRegistry API. It was written to find
 * a hard to reproduce bug in the L10nRegistry code. If it fails, place the seed
 * from the failing run in the code directly below to make it consistently reproducible.
 */
let seed = Math.floor(Math.random() * 1e9);

console.log(`Starting a fuzzing run with seed: ${seed}.`);
console.log("To reproduce this test locally, re-run it locally with:");
console.log(`let seed = ${seed};`);

/**
 * A simple non-robust psuedo-random number generator.
 *
 * It is implemented using a Lehmer random number generator.
 * https://en.wikipedia.org/wiki/16,807
 *
 * @returns {number} Ranged [0, 1)
 */
function prng() {
  const multiplier = 16807;
  const prime = 2147483647;
  seed = seed * multiplier % prime
  return (seed - 1) / prime
}

/**
 * Generate a name like "mock-dmsxfodrqboljmxdeayt".
 * @returns {string}
 */
function generateRandomName() {
  let name = 'mock-'
  const letters = "abcdefghijklmnopqrstuvwxyz";
  for (let i = 0; i < 20; i++) {
    name += letters[Math.floor(prng() * letters.length)];
  }
  return name;
}

/**
 * Picks one item from an array.
 *
 * @param {Array<T>}
 * @returns {T}
 */
function pickOne(list) {
  return list[Math.floor(prng() * list.length)]
}

/**
 * Picks a random subset from an array.
 *
 * @param {Array<T>}
 * @returns {Array<T>}
 */
function pickN(list, count) {
  list = list.slice();
  const result = [];
  for (let i = 0; i < count && i < list.length; i++) {
    // Pick a random item.
    const index = Math.floor(prng() * list.length);

    // Swap item to the end.
    const a = list[index];
    const b = list[list.length - 1];
    list[index] = b;
    list[list.length - 1] = a

    // Now that the random item is on the end, pop it off and add it to the results.
    result.push(list.pop());
  }

  return result
}

/**
 * Generate a random number
 * @param {number} min
 * @param {number} max
 * @returns {number}
 */
function random(min, max) {
  const delta = max - min;
  return min + delta * prng();
}

/**
 * Generate a random number generator with a distribution more towards the lower end.
 * @param {number} min
 * @param {number} max
 * @returns {number}
 */
function randomPow(min, max) {
  const delta = max - min;
  const r = prng()
  return min + delta * r * r;
}

add_task(async function test_fuzzing_sources() {
    const iterations = 100;
    const maxSources = 10;
  
    const metasources = ["app", "langpack", ""];
    const availableLocales = ["en", "en-US", "pl", "en-CA", "es-AR", "es-ES"];

    const l10nReg = new L10nRegistry();

    for (let i = 0; i < iterations; i++) {
      console.log("----------------------------------------------------------------------");
      console.log("Iteration", i);
      let sourceCount = randomPow(0, maxSources);

      const mocks = [];
      const fs = [];

      const locales = new Set();
      const filenames = new Set();

      for (let j = 0; j < sourceCount; j++) {
        const locale = pickOne(availableLocales);
        locales.add(locale);

        let metasource = pickOne(metasources);
        if (metasource === "langpack") {
          metasource = `${metasource}-${locale}`
        }
  
        const dir = generateRandomName();
        const filename = generateRandomName() + j + ".ftl";
        const path = `${dir}/${locale}/${filename}`
        const name = metasource || "app";
        const source = "key = value";

        filenames.add(filename);

        console.log("Add source", { name, metasource, path, source });
        fs.push({ path, source });

        mocks.push([
          name, // name
          metasource, // metasource,
          [locale], // locales,
          dir + "/{locale}/",
          fs
        ])
      }

      l10nReg.registerSources(mocks.map(args => L10nFileSource.createMock(...args)));

      const bundleLocales = pickN([...locales], random(1, 4));
      const bundleFilenames = pickN([...filenames], random(1, 10));

      console.log("generateBundles", {bundleLocales, bundleFilenames});
      const bundles = l10nReg.generateBundles(
        bundleLocales,
        bundleFilenames
      );

      function next() {
        console.log("Getting next bundle");
        const bundle = bundles.next()
        console.log("Next bundle obtained", bundle);
        return bundle;
      }

      const ops = [
        // Increase the frequency of next being called.
        next,
        next,
        next,
        () => {
          const newMocks = [];
          for (const mock of pickN(mocks, random(0, 3))) {
            const newMock = mock.slice();
            newMocks.push(newMock)
          }
          console.log("l10nReg.updateSources");
          l10nReg.updateSources(newMocks.map(mock => L10nFileSource.createMock(...mock)));
        },
        () => {
          console.log("l10nReg.clearSources");
          l10nReg.clearSources();
        }
      ];

      console.log("Start the operation loop");
      while (true) {
        console.log("Next operation");
        const op = pickOne(ops);
        const result = await op();
        if (result?.done) {
          // The iterator completed.
          break;
        }
      }

      console.log("Clear sources");
      l10nReg.clearSources();
    }

    ok(true, "The L10nRegistry fuzzing did not crash.")
});
