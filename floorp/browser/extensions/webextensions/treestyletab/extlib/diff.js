/**
 * Mainly ported from difflib.py, the standard diff library of Python.
 * This code is distributed under the Python Software Foundation License.
 * Contributor(s): Sutou Kouhei <kou@clear-code.com> (porting)
 *                 YUKI "Piro" Hiroshi <yuki@clear-code.com> (encoded diff)
 * ------------------------------------------------------------------------
 * Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009
 * Python Software Foundation.
 * All rights reserved.
 *
 * Copyright (c) 2000 BeOpen.com.
 * All rights reserved.
 *
 * Copyright (c) 1995-2001 Corporation for National Research Initiatives.
 * All rights reserved.
 *
 * Copyright (c) 1991-1995 Stichting Mathematisch Centrum.
 * All rights reserved.
 */

export class SequenceMatcher {
  constructor(from, to, junkPredicate) {
    this.from = from;
    this.to = to;
    this.junkPredicate = junkPredicate;
    this._updateToIndexes();
  }

  longestMatch(fromStart, fromEnd, toStart, toEnd) {
    let bestInfo = this._findBestMatchPosition(fromStart, fromEnd,
                                               toStart, toEnd);
    const haveJunk = Object.keys(this.junks).length > 0;
    if (haveJunk) {
      const adjust = this._adjustBestInfoWithJunkPredicate;
      const args = [fromStart, fromEnd, toStart, toEnd];

      bestInfo = adjust.apply(this, [false, bestInfo].concat(args));
      bestInfo = adjust.apply(this, [true, bestInfo].concat(args));
    }

    return bestInfo;
  }

  matches() {
    if (!this._matches)
      this._matches = this._computeMatches();
    return this._matches;
  }

  blocks() {
    if (!this._blocks)
      this._blocks = this._computeBlocks();
    return this._blocks;
  }

  operations() {
    if (!this._operations)
      this._operations = this._computeOperations();
    return this._operations;
  }

  groupedOperations(contextSize) {
    if (!contextSize)
      contextSize = 3;

    let operations = this.operations();
    if (operations.length == 0)
      operations = [['equal', 0, 0, 0, 0]];
    operations = this._expandEdgeEqualOperations(operations, contextSize);

    const groupWindow = contextSize * 2;
    const groups = [];
    let group = [];
    for (const operation of operations) {
      const tag = operation[0];
      let fromStart = operation[1];
      const fromEnd = operation[2];
      let toStart = operation[3];
      const toEnd = operation[4];

      if (tag == 'equal' && fromEnd - fromStart > groupWindow) {
        group.push([tag,
          fromStart,
          Math.min(fromEnd, fromStart + contextSize),
          toStart,
          Math.min(toEnd, toStart + contextSize)]);
        groups.push(group);
        group = [];
        fromStart = Math.max(fromStart, fromEnd - contextSize);
        toStart = Math.max(toStart, toEnd - contextSize);
      }
      group.push([tag, fromStart, fromEnd, toStart, toEnd]);
    }

    if (group.length > 0)
      groups.push(group);

    return groups;
  }

  ratio() {
    if (!this._ratio)
      this._ratio = this._computeRatio();
    return this._ratio;
  }

  _updateToIndexes() {
    this.toIndexes = {};
    this.junks = {};

    for (let i = 0, length = this.to.length; i < length; i++) {
      const item = this.to[i];

      if (!this.toIndexes[item])
        this.toIndexes[item] = [];
      this.toIndexes[item].push(i);
    }

    if (!this.junkPredicate)
      return;

    const toIndexesWithoutJunk = {};
    for (const item in this.toIndexes) {
      if (this.junkPredicate(item)) {
        this.junks[item] = true;
      } else {
        toIndexesWithoutJunk[item] = this.toIndexes[item];
      }
    }
    this.toIndexes = toIndexesWithoutJunk;
  }

  _findBestMatchPosition(fromStart, fromEnd, toStart, toEnd) {
    let bestFrom = fromStart;
    let bestTo = toStart;
    let bestSize = 0;
    let lastSizes = {};
    let fromIndex;

    for (fromIndex = fromStart; fromIndex <= fromEnd; fromIndex++) {
      const sizes = {};

      const toIndexes = this.toIndexes[this.from[fromIndex]] || [];
      for (let i = 0, length = toIndexes.length; i < length; i++) {
        const toIndex = toIndexes[i];

        if (toIndex < toStart)
          continue;
        if (toIndex > toEnd)
          break;

        const size = sizes[toIndex] = (lastSizes[toIndex - 1] || 0) + 1;
        if (size > bestSize) {
          bestFrom = fromIndex - size + 1;
          bestTo = toIndex - size + 1;
          bestSize = size;
        }
      }
      lastSizes = sizes;
    }

    return [bestFrom, bestTo, bestSize];
  }

  _adjustBestInfoWithJunkPredicate(shouldJunk, bestInfo,
    fromStart, fromEnd,
    toStart, toEnd) {
    let [bestFrom, bestTo, bestSize] = bestInfo;

    while (bestFrom > fromStart &&
           bestTo > toStart &&
           (shouldJunk ?
             this.junks[this.to[bestTo - 1]] :
             !this.junks[this.to[bestTo - 1]]) &&
           this.from[bestFrom - 1] == this.to[bestTo - 1]) {
      bestFrom -= 1;
      bestTo -= 1;
      bestSize += 1;
    }

    while (bestFrom + bestSize < fromEnd &&
           bestTo + bestSize < toEnd &&
           (shouldJunk ?
             this.junks[this.to[bestTo + bestSize]] :
             !this.junks[this.to[bestTo + bestSize]]) &&
           this.from[bestFrom + bestSize] == this.to[bestTo + bestSize]) {
      bestSize += 1;
    }

    return [bestFrom, bestTo, bestSize];
  }

  _computeMatches() {
    const matches = [];
    const queue = [[0, this.from.length, 0, this.to.length]];

    while (queue.length > 0) {
      const target = queue.pop();
      const [fromStart, fromEnd, toStart, toEnd] = target;

      const match = this.longestMatch(fromStart, fromEnd - 1, toStart, toEnd - 1);
      const matchFromIndex = match[0];
      const matchToIndex = match[1];
      const size = match[2];
      if (size > 0) {
        if (fromStart < matchFromIndex && toStart < matchToIndex)
          queue.push([fromStart, matchFromIndex, toStart, matchToIndex]);

        matches.push(match);
        if (matchFromIndex + size < fromEnd && matchToIndex + size < toEnd)
          queue.push([matchFromIndex + size, fromEnd,
            matchToIndex + size, toEnd]);
      }
    }

    matches.sort((matchInfo1, matchInfo2) => {
      const fromIndex1 = matchInfo1[0];
      const fromIndex2 = matchInfo2[0];
      return fromIndex1 - fromIndex2;
    });
    return matches;
  }

  _computeBlocks() {
    const blocks = [];
    let currentFromIndex = 0;
    let currentToIndex = 0;
    let currentSize = 0;

    for (const match of this.matches()) {
      const [fromIndex, toIndex, size] = match;

      if (currentFromIndex + currentSize == fromIndex &&
            currentToIndex + currentSize == toIndex) {
        currentSize += size;
      } else {
        if (currentSize > 0)
          blocks.push([currentFromIndex, currentToIndex, currentSize]);
        currentFromIndex = fromIndex;
        currentToIndex = toIndex;
        currentSize = size;
      }
    }

    if (currentSize > 0)
      blocks.push([currentFromIndex, currentToIndex, currentSize]);

    blocks.push([this.from.length, this.to.length, 0]);
    return blocks;
  }

  _computeOperations() {
    let fromIndex = 0;
    let toIndex = 0;
    const operations = [];

    for (const block of this.blocks()) {
      const [matchFromIndex, matchToIndex, size] = block;

      const tag = this._determineTag(fromIndex, toIndex,
                                     matchFromIndex, matchToIndex);
      if (tag != 'equal')
        operations.push([tag,
          fromIndex, matchFromIndex,
          toIndex, matchToIndex]);

      fromIndex = matchFromIndex + size;
      toIndex = matchToIndex + size;

      if (size > 0)
        operations.push(['equal',
          matchFromIndex, fromIndex,
          matchToIndex, toIndex]);
    }

    return operations;
  }

  _determineTag(fromIndex, toIndex,
    matchFromIndex, matchToIndex) {
    if (fromIndex < matchFromIndex && toIndex < matchToIndex) {
      return 'replace';
    } else if (fromIndex < matchFromIndex) {
      return 'delete';
    } else if (toIndex < matchToIndex) {
      return 'insert';
    } else {
      return 'equal';
    }
  }

  _expandEdgeEqualOperations(operations, contextSize) {
    const expandedOperations = [];

    for (let index = 0, length = operations.length; index < length; index++) {
      const operation = operations[index];
      const [tag, fromStart, fromEnd, toStart, toEnd] = operation;

      if (tag == 'equal' && index == 0) {
        expandedOperations.push([tag,
          Math.max(fromStart, fromEnd - contextSize),
          fromEnd,
          Math.max(toStart, toEnd - contextSize),
          toEnd]);
      } else if (tag == 'equal' && index == length - 1) {
        expandedOperations.push([tag,
          fromStart,
          Math.min(fromEnd, fromStart + contextSize),
          toStart,
          Math.min(toEnd, toStart + contextSize),
          toEnd]);
      } else {
        expandedOperations.push(operation);
      }
    }

    return expandedOperations;
  }

  _computeRatio() {
    const length = this.from.length + this.to.length;

    if (length == 0)
      return 1.0;

    let matches = 0;
    for (const block of this.blocks()) {
      const size = block[2];
      matches += size;
    }
    return 2.0 * matches / length;
  }

};


export class ReadableDiffer {
  constructor(from, to) {
    this.from = from;
    this.to = to;
  }

  diff() {
    let lines = [];
    const matcher = new SequenceMatcher(this.from, this.to);

    for (const operation of matcher.operations()) {
      const [tag, fromStart, fromEnd, toStart, toEnd] = operation;
      let target;

      switch (tag) {
        case 'replace':
          target = this._diffLines(fromStart, fromEnd, toStart, toEnd);
          lines = lines.concat(target);
          break;
        case 'delete':
          target = this.from.slice(fromStart, fromEnd);
          lines = lines.concat(this._tagDeleted(target));
          break;
        case 'insert':
          target = this.to.slice(toStart, toEnd);
          lines = lines.concat(this._tagInserted(target));
          break;
        case 'equal':
          target = this.from.slice(fromStart, fromEnd);
          lines = lines.concat(this._tagEqual(target));
          break;
        default:
          throw 'unknown tag: ' + tag;
          break;
      }
    }

    return lines;
  }

  encodedDiff() {
    let lines = [];
    const matcher = new SequenceMatcher(this.from, this.to);

    for (const operation of matcher.operations()) {
      const [tag, fromStart, fromEnd, toStart, toEnd] = operation;
      let target;

      switch (tag) {
        case 'replace':
          target = this._diffLines(fromStart, fromEnd, toStart, toEnd, true);
          lines = lines.concat(target);
          break;
        case 'delete':
          target = this.from.slice(fromStart, fromEnd);
          lines = lines.concat(this._tagDeleted(target, true));
          break;
        case 'insert':
          target = this.to.slice(toStart, toEnd);
          lines = lines.concat(this._tagInserted(target, true));
          break;
        case 'equal':
          target = this.from.slice(fromStart, fromEnd);
          lines = lines.concat(this._tagEqual(target, true));
          break;
        default:
          throw new Error(`unknown tag: ${tag}`);
          break;
      }
    }

    const blocks = [];
    let lastBlock = '';
    let lastLineType = '';
    for (const line of lines) {
      const lineType = line.match(/^<span class="line ([^" ]+)/)[1];
      if (lineType != lastLineType) {
        blocks.push(lastBlock + (lastBlock ? '</span>' : '' ));
        lastBlock = `<span class="block ${lineType}">`;
        lastLineType = lineType;
      }
      lastBlock += line;
    }
    if (lastBlock)
      blocks.push(`${lastBlock}</span>`);

    return blocks.join('');
  }

  _tagLine(mark, contents) {
    return contents.map(content => `${mark} ${content}`);
  }

  _encodedTagLine(encodedClass, contents) {
    return contents.map(content => `<span class="line ${encodedClass}">${this._escapeForEncoded(content)}</span>`);
  }

  _escapeForEncoded(string) {
    return string
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;');
  }

  _tagDeleted(contents, encoded) {
    return encoded ?
      this._encodedTagLine('deleted', contents) :
      this._tagLine('-', contents);
  }

  _tagInserted(contents, encoded) {
    return encoded ?
      this._encodedTagLine('inserted', contents) :
      this._tagLine('+', contents);
  }

  _tagEqual(contents, encoded) {
    return encoded ?
      this._encodedTagLine('equal', contents) :
      this._tagLine(' ', contents);
  }

  _tagDifference(contents, encoded) {
    return encoded ?
      this._encodedTagLine('difference', contents) :
      this._tagLine('?', contents);
  }

  _findDiffLineInfo(fromStart, fromEnd, toStart, toEnd) {
    let bestRatio = 0.74;
    let fromEqualIndex, toEqualIndex;
    let fromBestIndex, toBestIndex;

    for (let toIndex = toStart; toIndex < toEnd; toIndex++) {
      for (let fromIndex = fromStart; fromIndex < fromEnd; fromIndex++) {
        if (this.from[fromIndex] == this.to[toIndex]) {
          if (fromEqualIndex === undefined)
            fromEqualIndex = fromIndex;
          if (toEqualIndex === undefined)
            toEqualIndex = toIndex;
          continue;
        }

        const matcher = new SequenceMatcher(this.from[fromIndex],
                                            this.to[toIndex],
                                            this._isSpaceCharacter);
        if (matcher.ratio() > bestRatio) {
          bestRatio = matcher.ratio();
          fromBestIndex = fromIndex;
          toBestIndex = toIndex;
        }
      }
    }

    return [bestRatio,
      fromEqualIndex, toEqualIndex,
      fromBestIndex, toBestIndex];
  }

  _diffLines(fromStart, fromEnd, toStart, toEnd, encoded) {
    const cutOff = 0.75;
    const info = this._findDiffLineInfo(fromStart, fromEnd, toStart, toEnd);
    let bestRatio = info[0];
    const fromEqualIndex = info[1];
    const toEqualIndex = info[2];
    let fromBestIndex = info[3];
    let toBestIndex = info[4];

    if (bestRatio < cutOff) {
      if (fromEqualIndex === undefined) {
        const taggedFrom = this._tagDeleted(this.from.slice(fromStart, fromEnd), encoded);
        const taggedTo = this._tagInserted(this.to.slice(toStart, toEnd), encoded);
        if (toEnd - toStart < fromEnd - fromStart)
          return taggedTo.concat(taggedFrom);
        else
          return taggedFrom.concat(taggedTo);
      }

      fromBestIndex = fromEqualIndex;
      toBestIndex = toEqualIndex;
      bestRatio = 1.0;
    }

    return [].concat(
      this.__diffLines(fromStart, fromBestIndex,
                       toStart, toBestIndex,
                       encoded),
      (encoded ?
        this._diffLineEncoded(this.from[fromBestIndex],
                              this.to[toBestIndex]) :
        this._diffLine(this.from[fromBestIndex],
                       this.to[toBestIndex])
      ),
      this.__diffLines(fromBestIndex + 1, fromEnd,
                       toBestIndex + 1, toEnd,
                       encoded)
    );
  }

  __diffLines(fromStart, fromEnd, toStart, toEnd, encoded) {
    if (fromStart < fromEnd) {
      if (toStart < toEnd) {
        return this._diffLines(fromStart, fromEnd, toStart, toEnd, encoded);
      } else {
        return this._tagDeleted(this.from.slice(fromStart, fromEnd), encoded);
      }
    } else {
      return this._tagInserted(this.to.slice(toStart, toEnd), encoded);
    }
  }

  _diffLineEncoded(fromLine, toLine) {
    const fromChars = fromLine.split('');
    const toChars = toLine.split('');
    const matcher = new SequenceMatcher(fromLine, toLine,
                                        this._isSpaceCharacter);
    const phrases = [];
    for (const operation of matcher.operations()) {
      const [tag, fromStart, fromEnd, toStart, toEnd] = operation;
      const fromPhrase = fromChars.slice(fromStart, fromEnd).join('');
      const toPhrase = toChars.slice(toStart, toEnd).join('');
      switch (tag) {
        case 'replace':
        case 'delete':
        case 'insert':
        case 'equal':
          phrases.push({ tag         : tag,
            from        : fromPhrase,
            encodedFrom : this._escapeForEncoded(fromPhrase),
            to          : toPhrase,
            encodedTo   : this._escapeForEncoded(toPhrase), });
          break;
        default:
          throw new Error(`unknown tag: ${tag}`);
      }
    }

    const encodedPhrases = [];
    let current;
    let replaced = 0;
    let inserted = 0;
    let deleted = 0;
    for (let i = 0, maxi = phrases.length; i < maxi; i++)
    {
      current = phrases[i];
      switch (current.tag) {
        case 'replace':
          encodedPhrases.push('<span class="phrase replaced">');
          encodedPhrases.push(this._encodedTagPhrase('deleted', current.encodedFrom));
          encodedPhrases.push(this._encodedTagPhrase('inserted', current.encodedTo));
          encodedPhrases.push('</span>');
          replaced++;
          break;
        case 'delete':
          encodedPhrases.push(this._encodedTagPhrase('deleted', current.encodedFrom));
          deleted++;
          break;
        case 'insert':
          encodedPhrases.push(this._encodedTagPhrase('inserted', current.encodedTo));
          inserted++;
          break;
        case 'equal':
          // \95ύX\93_\82̊Ԃɋ\B2\82܂ꂽ1\95\B6\8E\9A\82\BE\82\AF\82̖\B3\95ύX\95\94\95\AA\82\BE\82\AF\82͓\C1\95ʈ\B5\82\A2
          if (
            current.from.length == 1 &&
                    (i > 0 && phrases[i-1].tag != 'equal') &&
                    (i < maxi-1 && phrases[i+1].tag != 'equal')
          ) {
            encodedPhrases.push('<span class="phrase equal">');
            encodedPhrases.push(this._encodedTagPhrase('duplicated', current.encodedFrom));
            encodedPhrases.push(this._encodedTagPhrase('duplicated', current.encodedTo));
            encodedPhrases.push('</span>');
          }
          else {
            encodedPhrases.push(current.encodedFrom);
          }
          break;
      }
    }

    const extraClass = (replaced || (deleted && inserted)) ?
      ' includes-both-modification' :
      '' ;

    return [
      `<span class="line replaced${extraClass}">${encodedPhrases.join('')}</span>`
    ];
  }

  _encodedTagPhrase(encodedClass, content) {
    return `<span class="phrase ${encodedClass}">${content}</span>`;
  }

  _diffLine(fromLine, toLine) {
    let fromTags = '';
    let toTags = '';
    const matcher = new SequenceMatcher(fromLine, toLine,
                                        this._isSpaceCharacter);

    for (const operation of matcher.operations()) {
      const [tag, fromStart, fromEnd, toStart, toEnd] = operation;

      const fromLength = fromEnd - fromStart;
      const toLength = toEnd - toStart;
      switch (tag) {
        case 'replace':
          fromTags += this._repeat('^', fromLength);
          toTags += this._repeat('^', toLength);
          break;
        case 'delete':
          fromTags += this._repeat('-', fromLength);
          break;
        case 'insert':
          toTags += this._repeat('+', toLength);
          break;
        case 'equal':
          fromTags += this._repeat(' ', fromLength);
          toTags += this._repeat(' ', toLength);
          break;
        default:
          throw new Error(`unknown tag: ${tag}`);
          break;
      }
    }

    return this._formatDiffPoint(fromLine, toLine, fromTags, toTags);
  }

  _formatDiffPoint(fromLine, toLine, fromTags, toTags) {
    let common;
    let result;

    common = Math.min(this._nLeadingCharacters(fromLine, '\t'),
                      this._nLeadingCharacters(toLine, '\t'));
    common = Math.min(common,
                      this._nLeadingCharacters(fromTags.slice(0, common),
                                               ' '));
    fromTags = fromTags.slice(common).replace(/\s*$/, '');
    toTags = toTags.slice(common).replace(/\s*$/, '');

    result = this._tagDeleted([fromLine]);
    if (fromTags.length > 0) {
      fromTags = this._repeat('\t', common) + fromTags;
      result = result.concat(this._tagDifference([fromTags]));
    }
    result = result.concat(this._tagInserted([toLine]));
    if (toTags.length > 0) {
      toTags = this._repeat('\t', common) + toTags;
      result = result.concat(this._tagDifference([toTags]));
    }

    return result;
  }

  _nLeadingCharacters(string, character) {
    let n = 0;
    while (string[n] == character) {
      n++;
    }
    return n;
  }

  _isSpaceCharacter(character) {
    return character == ' ' || character == '\t';
  }

  _repeat(string, n) {
    let result = '';
    for (; n > 0; n--) {
      result += string;
    }
    return result;
  }

};


export const Diff = {

  readable(from, to, encoded) {
    const differ = new ReadableDiffer(this._splitWithLine(from), this._splitWithLine(to));
    return encoded ?
      differ.encodedDiff() :
      differ.diff(encoded).join('\n') ;
  },

  foldedReadable(from, to, encoded) {
    const differ = new ReadableDiffer(this._splitWithLine(this._fold(from)),
                                      this._splitWithLine(this._fold(to)));
    return encoded ?
      differ.encodedDiff() :
      differ.diff(encoded).join('\n') ;
  },

  isInterested(diff) {
    if (!diff)
      return false;

    if (diff.length == 0)
      return false;

    if (!diff.match(/^[-+]/mg))
      return false;

    if (diff.match(/^[ ?]/mg))
      return true;

    if (diff.match(/(?:.*\n){2,}/g))
      return true;

    if (this.needFold(diff))
      return true;

    return false;
  },

  needFold(diff) {
    if (!diff)
      return false;

    if (diff.match(/^[-+].{79}/mg))
      return true;

    return false;
  },

  _splitWithLine(string) {
    string = String(string);
    return string.length == 0 ? [] : string.split(/\r?\n/);
  },

  _fold(string) {
    string = String(string);
    const foldedLines = string.split('\n').map(line => line.replace(/(.{78})/g, '$1\n'));
    return foldedLines.join('\n');
  }

};
