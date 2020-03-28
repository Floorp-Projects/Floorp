/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// We load this into a worker using importScripts, and in tests using import.
// We use var to avoid name collision errors.
// eslint-disable-next-line no-var
var EXPORTED_SYMBOLS = ["NaiveBayesTextTagger"];

const NaiveBayesTextTagger = class NaiveBayesTextTagger {
  constructor(model, toksToTfIdfVector) {
    this.model = model;
    this.toksToTfIdfVector = toksToTfIdfVector;
  }

  /**
   * Determines if the tokenized text belongs to class according to binary naive Bayes
   * classifier. Returns an object containing the class label ("label"), and
   * the log probability ("logProb") that the text belongs to that class. If
   * the positive class is more likely, then "label" is the positive class
   * label. If the negative class is matched, then "label" is set to null.
   */
  tagTokens(tokens) {
    let fv = this.toksToTfIdfVector(tokens, this.model.vocab_idfs);

    let bestLogProb = null;
    let bestClassId = -1;
    let bestClassLabel = null;
    let logSumExp = 0.0; // will be P(x). Used to create a proper probability
    for (let classId = 0; classId < this.model.classes.length; classId++) {
      let classModel = this.model.classes[classId];
      let classLogProb = classModel.log_prior;

      // dot fv with the class model
      for (let pair of Object.values(fv)) {
        let [termId, tfidf] = pair;
        classLogProb += tfidf * classModel.feature_log_probs[termId];
      }

      if (bestLogProb === null || classLogProb > bestLogProb) {
        bestLogProb = classLogProb;
        bestClassId = classId;
      }
      logSumExp += Math.exp(classLogProb);
    }

    // now normalize the probability by dividing by P(x)
    logSumExp = Math.log(logSumExp);
    bestLogProb -= logSumExp;
    if (bestClassId === this.model.positive_class_id) {
      bestClassLabel = this.model.positive_class_label;
    } else {
      bestClassLabel = null;
    }

    let confident =
      bestClassId === this.model.positive_class_id &&
      bestLogProb > this.model.positive_class_threshold_log_prob;
    return {
      label: bestClassLabel,
      logProb: bestLogProb,
      confident,
    };
  }
};
