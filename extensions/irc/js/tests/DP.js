/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is JSIRC Library.
 *
 * The Initial Developer of the Original Code is
 * New Dimensions Consulting, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, rginda@ndcico.com, original author
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Dissociated Press javascript for the jsbot
 * see: http://wombat.doc.ic.ac.uk/foldoc/foldoc.cgi?query=dissociated%20press
 */

DP_DEBUG = false;

if (DP_DEBUG)
    dpprint = dd;
else
    dpprint = (function () {});

function CDPressMachine()
{

    this.wordPivots = new Object();
    this.cleanCounter = 0;
    this.cleanCount = 0;

}

CDPressMachine.CLEAN_CYCLE = 1000;     // list will be trimmed after this many
                                       // or never if < 1 addPhrase()s
CDPressMachine.CLEAN_THRESHOLD = 2;    // anything <= this will be trimmed
CDPressMachine.RANDOMIZE_DEPTH = 10;   // not used yet
CDPressMachine.MIN_PHRASE_LENGTH = 3;  // requested minimum phrase length
CDPressMachine.MAX_PHRASE_LENGTH = 8;  // requested maximum phrase length
CDPressMachine.LENGTH_RETRIES = 3      // number of retries per word
                                       // (to reach maxlen)
CDPressMachine.WORD_PATTERN = /[\x21-\x7e]+/;  // pattern for words

/**
 * Adds a phrase to the engine
 */
CDPressMachine.prototype.addPhrase =
function DPM_addPhrase (strPhrase, weight)
{
    if (strPhrase == "")
        return;

    this.cleanCounter++;
    if ((CDPressMachine.CLEAN_CYCLE >= 1) &&
        (this.cleanCounter >= CDPressMachine.CLEAN_CYCLE))
    {
        dpprint ("** cleaning list");
        
        this.cleanCounter = 0;
        this.trimList (CDPressMachine.CLEAN_THRESHOLD);
        this.cleancount++;
    }

    strPhrase = strPhrase.toLowerCase();

        /* split the phrase */
    var aryWordMatches = strPhrase.split (" ");
    var previousWord = aryWordMatches[aryWordMatches.length - 1];
    previousWord = previousWord.match(CDPressMachine.WORD_PATTERN);
    var nextWord = "";

        /* loop through each word */
    for (var i=-1; i < aryWordMatches.length; i++)
    {
        var currentWord = nextWord;       
        var currentWordPivot = this.wordPivots[currentWord];

        if (typeof currentWordPivot == "undefined")
            currentWordPivot = 
                (this.wordPivots[currentWord] = new CWordPivot (currentWord));

        currentWordPivot.previousList.addLink (previousWord, weight);

        if (i < aryWordMatches.length - 1)
        {
            nextWord = aryWordMatches[i + 1];
            if (nextWord == (String.fromCharCode(1) + "action"))
                nextWord = escape(nextWord.toUpperCase());
            else
                nextWord = nextWord.match(CDPressMachine.WORD_PATTERN);

            if (nextWord == null)
                nextWord = "";                                  //this is weak
	
            currentWordPivot.nextList.addLink (nextWord, weight);
        }
        else
            currentWordPivot.nextList.addLink ("");

        previousWord = currentWord;
        
    }

}

CDPressMachine.prototype.addPhrases =
function DPM_addPhrases(phrases)
{

    for (var i in phrases)
        this.addPhrase (phrases[i]);
    
}

/**
 * Gets a phrase from the engine, starting from seedWord.
 * if dir is greater than 0, then seedWord will be the first in
 * the phrase, otherwise it will be the last
 */
CDPressMachine.prototype.getPhraseDirected =
function DPM_getPhraseDirected(seedWord, dir)
{
    var word = (typeof seedWord != "undefined") ? seedWord : "";
    var tempword = word;
    var rval = "";
    var c = 0, retry = 0;

    dpprint ("DPM_getPhraseDirected: '" + word + "' " + dir);
    
    if (typeof this.wordPivots[word] == "undefined")
        return;
    
    do
    {
        if (typeof this.wordPivots[word] == "undefined")
        {
            dd ("** DP Error: Word '" + word + "' is not a pivot **");
            return;
        }
    
        if (dir > 0)     // pick a word
            word= this.wordPivots[word].nextList.getRandomLink().link;
        else
            word= this.wordPivots[word].previousList.getRandomLink().link;
        
        if (word != "")  // if it isnt blank
        {
            dpprint ("DPM_getPhraseDirected: got word '" + word + "'");

            if (c < CDPressMachine.MIN_PHRASE_LENGTH)
                retry = 0;

            if (c > CDPressMachine.MAX_PHRASE_LENGTH)
                if (((dir > 0) && (this.wordPivots[word].nextList.list[""])) ||
                    ((dir <= 0) &&
                     (this.wordPivots[word].previousList.list[""])))
                {
                    dpprint ("DPM_getPhraseDirected: forcing last word");
                    word="";
                    rval = rval.substring (0, rval.length - 1);
                    break;
                }
            
            if (dir > 0)
                rval += word + " "; // put it in the rslt
            else
                rval = word + " " + rval;
            
            c++;                    // count the word
        }
        else                        // otherwise
        {
            dpprint ("DPM_getPhraseDirected: last word");
                // if it's too short
                // and were not out of retrys
            if ((c < CDPressMachine.MIN_PHRASE_LENGTH) &&  
                (retry++ < CDPressMachine.LENGTH_RETRIES))
                word = tempword; // try again
            else
                    // otherwise, we're done
                rval = rval.substring (0, rval.length - 1); 
        }
        
        tempword = word;
        
    } while (word != "");

    rval = unescape (rval);
    
    return rval;    

}

CDPressMachine.prototype.getPhraseForward =
function DPM_getPhraseForward(firstWord)
{
    return this.getPhraseDirected (firstWord, 1)       
}

CDPressMachine.prototype.getPhraseReverse =
function DPM_getPhraseReverse(lastWord)
{
    return this.getPhraseDirected (lastWord, -1)    
}

/**
 * locates a random pivot by following CDPressMachine.RANDOMIZE_DEPTH
 * links from |word|.
 */
CDPressMachine.prototype.getRandomPivot =
function DPM_getRandomPivot (word)
{
    
    /**
     * XXXrgg: erm, this is currently pointless, but could be neat later
     *         if max phrase length's were implemented.
     */
    if (false)
    {
        var depth = parseInt (Math.round
                              (CDPressMachine.RANDOMIZE_DEPTH * Math.random()));
        word = "";
        for (var i = 0;
             i < depth, word =
                 this.wordPivots[word].nextList.getRandomLink().link;
             i++); /* empty loop */
        
    }
    
}

CDPressMachine.prototype.getPhrase =
function DPM_getPhrase(word)
{
    var rval = this.getPhraseContaining (word);
    
    return rval;
    
}

/**
 * Returns a phrase with |word| somewhere in it.
 */
CDPressMachine.prototype.getPhraseContaining =
function DPM_getPhraseContaining(word)
{
    if (typeof word == "undefined")
        word = "";
    else
        word = word.toString();

    dpprint ("* DPM_getPhraseContaining: '" + word + "'");

    var rval, spc;
    var post, pre = this.getPhraseReverse (word);
    if (word != "")
        var post = this.getPhraseForward (word);

    dpprint ("* DPM_getPhraseContaining: pre = '" + pre + "' post = '" +
             post + "'");
    dpprint ("* DPM_getPhraseContaining: " + (post == "" && pre == ""));

    if (word)
    {
        word = unescape (word);
        spc = " ";
    }
    else
        spc = "";
    
    if (pre)
    {
        if (post)
            rval = pre + spc + word + spc + post;
        else
            rval = pre + spc + word;
    }       
    else
    {
        if (post)
            rval = word + spc + post;
        else
            if (post == "" && pre == "")
                rval = word;
    }
    
    if (rval && (rval.charCodeAt(0) == 1))
        rval += String.fromCharCode(1);

    dpprint ("* DPM_getPhraseContaining: returning '" + rval + "'");
    
    return rval;
    
}

CDPressMachine.prototype.getPhraseWeight =
function DPM_getPhraseWeight (phrase)
{
    var ary = this.getPhraseWeights (phrase);
    var w = 0;
    
    while (ary.length > 0)
        w += ary.pop();

    return w;
}

CDPressMachine.prototype.getPhraseWeights =
function DPM_getPhraseWeights (phrase)
{
    var words, ary = new Array();
    var lastword = "";
    var link, pivot;
    
    if (!phrase)
        return ary;
    
    words = phrase.split (" ");

    for (var i = 0; i < words.length; i++)
    {

        if (i == 0)
        {
            lastWord = "";
            nextWord = words[i + 1];
        }
        else if (i == words.length - 1)
        {
            lastWord = words[i - 1];
            nextWord = "";
        }
        else
        {
            lastWord = words[i - 1];
            nextWord = words[i + 1];
        }

        pivot = this.wordPivots[words[i]];

        if (pivot)
        {   
            link = pivot.previousList.list[lastWord];
        
            if (link)
                ary.push(link.weight);
            else
                ary.push(0);

            link = pivot.nextList.list[nextWord];
            
            if (link)
                ary.push(link.weight);
            else
                ary.push(0);
        }
        else
        {
            ary.push(0);
            ary.push(0);
        }   

    }        

    return ary;
    
}

CDPressMachine.prototype.getPivot =
function DPM_getPivot(word)
{

    return this.wordPivots[word];
    
}

CDPressMachine.prototype.trimList =
function DPM_trimList(threshold)
{
    var el;
    var c;
    
    for (el in this.wordPivots)
    {
        c = this.wordPivots[el].nextList.trimList (threshold);
        if (c == 0)
            delete this.wordPivots[el];
        else
        {
            c = this.wordPivots[el].previousList.trimList (threshold);
            if (c == 0)
                delete this.wordPivots[el];
        }
        
    }

}

CDPressMachine.prototype.getMachineStatus =
function DPM_getMachineStatus()
{
    var o = new Object();

    o.pivotcount = 0;
    o.linkcount = 0;
    o.linksperpivot = 0;
    o.maxweight = 0;
    o.minweight = Number.MAX_VALUE;
    o.averageweight = 0;
    o.cleancounter = this.cleanCounter;
    o.cleancount = this.cleanCount;

    for (var pivot in this.wordPivots)
    {
        o.pivotcount++;

        for (var link in this.wordPivots[pivot].previousList.list)
        {
            var l = this.wordPivots[pivot].previousList.list[link];

            o.linkcount++;
            
            o.maxweight = Math.max (o.maxweight, l.weight);
            o.minweight = Math.min (o.minweight, l.weight);
            
            (o.averageweight == 0) ?
                o.averageweight = l.weight :
                o.averageweight = (l.weight + o.averageweight) / 2;
            
        }
    }    

    o.linksperpivot = o.linkcount / o.pivotcount;

    return o;
    
}

////////////////////////
function CWordPivot (word)
{

    dpprint ("* new pivot : '" + word + "'");
    this.word = word;
    this.nextList = new CPhraseLinkList(word, "next");
    this.previousList = new CPhraseLinkList(word, "prevoius");

}

///////////////////////

function CPhraseLinkList (parentWord, listID)
{

    if (DP_DEBUG)
    {
        this.parentWord = parentWord;
        this.listID = listID;
    }
    
    this.list = new Object();

}

CPhraseLinkList.prototype.addLink =
function PLL_addLink (link, weight)
{
    var existingLink = this.list[link];
    
    dpprint ("* adding link to '" + link + "' from '" + this.parentWord +
             "' in list '" + this.listID + "'");
    
    if (typeof weight == "undefined")
        weight = 1;

    if (typeof existingLink == "undefined")
        this.list[link] = new CPhraseLink (link, weight);
    else
        if (!(typeof existingLink.adjust == "function"))
            dd("existingLink.adjust is a '" + existingLink.adjust + "' " +
               "not a function! link is '" + link +"'");
        else
            existingLink.adjust (weight);

}

CPhraseLinkList.prototype.getRandomLink =
function PLL_getRandomLink ()
{
    var tot = 0;
    var lastMatch = "";
    var aryChoices = new Array();
    var fDone = false;

    dpprint ("* PLL_getRandomLink: from '" + this.parentWord + "'");
    
    for (el in this.list)
    {
        tot += this.list[el].weight;
        
        for (var i = 0; i< aryChoices.length; i++)
            if (this.list[el].weight <= aryChoices[i].weight)
                break;
        
        arrayInsertAt (aryChoices, i, this.list[el]);
    }

    if (DP_DEBUG)
        for (var i = 0; i < aryChoices.length; i++)
            dpprint ("** potential word '" + aryChoices[i].link + "', weight " +
                     aryChoices[i].weight);

    var choice = parseInt (Math.round(((tot - 1) * Math.random()) + 1));

    dpprint ("* PLL_getRandomLink: tot = " + tot + ", choice = " + choice);

    tot = 0;
    for (i = 0; i < aryChoices.length; i++)
    {
        if ((tot += aryChoices[i].weight) >= choice)
        {
            lastMatch =  aryChoices[i];
            break;
        }
    }
    
    if (lastMatch == "")
        lastMatch =  aryChoices[aryChoices.length - 1];

    if (!lastMatch)
        lastMatch = {link: ""}

    dpprint ("* PLL_getRandomLink: returning: " + lastMatch);
    
    return lastMatch;
    
}

CPhraseLinkList.prototype.getListWeights =
function PLL_getListWeights ()
{
    var ary = new Array();

    for (var el in this.list)
        ary.push (this.list[el].weight);

    return ary;
    
}

CPhraseLinkList.prototype.getListLinks =
function PLL_getListLinks ()
{
    var ary = new Array();

    for (var el in this.list)
        ary.push (this.list[el].link);

    return ary;
    
}
    
CPhraseLinkList.prototype.trimList =
function PLL_trimList (threshold)
{
    var el;
    var c;
    
    dpprint ("trimming '" + this.parentWord + "'s list to " + threshold);
    
    for (el in this.list)
    {
        c++;
        
        if (this.list[el].weight <= threshold)
        {
            dpprint ("removing '" + el + "' from '" + this.parentWord + "'s '" +
                     this.listID + "' list, because it's weight is " +
                     this.list[el].weight);
            
            delete this.list[el];
            c--;
        }
    }

    return c;
    
}   

////////////////////////

function CPhraseLink (link, weight)
{
    if (typeof weight == "undefined")
        this.weight = 1;
    else
        this.weight = weight;

    this.link = link;

}

CPhraseLink.prototype.adjust =
function PL_adjust(weight)
{
	
    if ((this.weight += weight) < 1)
        this.weight = 1;

}

CPhraseLink.prototype.weight =
function PL_weight ()
{

    return this.weight;

}
