import {
  tokenize,
  toksToTfIdfVector,
} from "lib/PersonalityProvider/Tokenize.jsm";

const EPSILON = 0.00001;

describe("TF-IDF Term Vectorizer", () => {
  describe("#tokenize", () => {
    let testCases = [
      { input: "HELLO there", expected: ["hello", "there"] },
      { input: "blah,,,blah,blah", expected: ["blah", "blah", "blah"] },
      {
        input: "Call Jenny: 967-5309",
        expected: ["call", "jenny", "967", "5309"],
      },
      {
        input: "Yo(what)[[hello]]{{jim}}}bob{1:2:1+2=$3",
        expected: [
          "yo",
          "what",
          "hello",
          "jim",
          "bob",
          "1",
          "2",
          "1",
          "2",
          "3",
        ],
      },
      { input: "čÄfė 80's", expected: ["čäfė", "80", "s"] },
      { input: "我知道很多东西。", expected: ["我知道很多东西"] },
    ];
    let checkTokenization = tc => {
      it(`${tc.input} should tokenize to ${tc.expected}`, () => {
        assert.deepEqual(tc.expected, tokenize(tc.input));
      });
    };

    for (let i = 0; i < testCases.length; i++) {
      checkTokenization(testCases[i]);
    }
  });

  describe("#tfidf", () => {
    let vocab_idfs = {
      deal: [221, 5.5058519847862275],
      easy: [269, 5.5058519847862275],
      tanks: [867, 5.601162164590552],
      sites: [792, 5.957837108529285],
      care: [153, 5.957837108529285],
      needs: [596, 5.824305715904762],
      finally: [334, 5.706522680248379],
    };
    let testCases = [
      {
        input: "Finally! Easy care for your tanks!",
        expected: {
          finally: [334, 0.5009816295853761],
          easy: [269, 0.48336453811728713],
          care: [153, 0.5230447876368227],
          tanks: [867, 0.49173191907236774],
        },
      },
      {
        input: "Easy easy EASY",
        expected: { easy: [269, 1.0] },
      },
      {
        input: "Easy easy care",
        expected: {
          easy: [269, 0.8795205218806832],
          care: [153, 0.4758609582543317],
        },
      },
      {
        input: "easy care",
        expected: {
          easy: [269, 0.6786999710383944],
          care: [153, 0.7344156515982504],
        },
      },
      {
        input: "这个空间故意留空。",
        expected: {
          /* This space is left intentionally blank. */
        },
      },
    ];
    let checkTokenGeneration = tc => {
      describe(`${tc.input} should have only vocabulary tokens`, () => {
        let actual = toksToTfIdfVector(tokenize(tc.input), vocab_idfs);

        it(`${tc.input} should generate exactly ${Object.keys(
          tc.expected
        )}`, () => {
          let seen = {};
          Object.keys(actual).forEach(actualTok => {
            assert.isTrue(actualTok in tc.expected);
            seen[actualTok] = true;
          });
          Object.keys(tc.expected).forEach(expectedTok => {
            assert.isTrue(expectedTok in seen);
          });
        });

        it(`${tc.input} should have the correct token ids`, () => {
          Object.keys(actual).forEach(actualTok => {
            assert.equal(tc.expected[actualTok][0], actual[actualTok][0]);
          });
        });
      });
    };

    let checkTfIdfVector = tc => {
      let actual = toksToTfIdfVector(tokenize(tc.input), vocab_idfs);
      it(`${tc.input} should have the correct tf-idf`, () => {
        Object.keys(actual).forEach(actualTok => {
          let delta = Math.abs(
            tc.expected[actualTok][1] - actual[actualTok][1]
          );
          assert.isTrue(delta <= EPSILON);
        });
      });
    };

    // run the tests
    for (let i = 0; i < testCases.length; i++) {
      checkTokenGeneration(testCases[i]);
      checkTfIdfVector(testCases[i]);
    }
  });
});
