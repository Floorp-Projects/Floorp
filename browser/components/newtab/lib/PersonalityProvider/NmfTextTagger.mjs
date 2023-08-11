/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class NmfTextTagger {
  constructor(model, toksToTfIdfVector) {
    this.model = model;
    this.toksToTfIdfVector = toksToTfIdfVector;
  }

  /**
   * A multiclass classifier that scores tokenized text against several classes through
   * inference of a nonnegative matrix factorization of TF-IDF vectors and
   * class labels. Returns a map of class labels as string keys to scores.
   * (Higher is more confident.) All classes get scored, so it is up to
   * consumer of this data determine what classes are most valuable.
   */
  tagTokens(tokens) {
    let fv = this.toksToTfIdfVector(tokens, this.model.vocab_idfs);
    let fve = Object.values(fv);

    // normalize by the sum of the vector
    let sum = 0.0;
    for (let pair of fve) {
      // eslint-disable-next-line prefer-destructuring
      sum += pair[1];
    }
    for (let i = 0; i < fve.length; i++) {
      // eslint-disable-next-line prefer-destructuring
      fve[i][1] /= sum;
    }

    // dot the document with each topic vector so that we can transform it into
    // the latent space
    let toksInLatentSpace = [];
    for (let topicVect of this.model.topic_word) {
      let fvDotTwv = 0;
      // dot fv with each topic word vector
      for (let pair of fve) {
        let [termId, tfidf] = pair;
        fvDotTwv += tfidf * topicVect[termId];
      }
      toksInLatentSpace.push(fvDotTwv);
    }

    // now project toksInLatentSpace back into class space
    let predictions = {};
    Object.keys(this.model.document_topic).forEach(topic => {
      let score = 0;
      for (let i = 0; i < toksInLatentSpace.length; i++) {
        score += toksInLatentSpace[i] * this.model.document_topic[topic][i];
      }
      predictions[topic] = score;
    });

    return predictions;
  }
}
