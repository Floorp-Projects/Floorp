/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function StarGeneratorNext(val) {
    if (!IsObject(this) || !IsStarGeneratorObject(this))
        return callFunction(CallStarGeneratorMethodIfWrapped, this, val, "StarGeneratorNext");

    if (StarGeneratorObjectIsClosed(this))
        return { value: undefined, done: true };

    if (GeneratorIsRunning(this))
        ThrowError(JSMSG_NESTING_GENERATOR);

    try {
        return resumeGenerator(this, val, 'next');
    } catch (e) {
        if (!StarGeneratorObjectIsClosed(this))
            GeneratorSetClosed(this);
        throw e;
    }
}

function StarGeneratorThrow(val) {
    if (!IsObject(this) || !IsStarGeneratorObject(this))
        return callFunction(CallStarGeneratorMethodIfWrapped, this, val, "StarGeneratorThrow");

    if (StarGeneratorObjectIsClosed(this))
        throw val;

    if (GeneratorIsRunning(this))
        ThrowError(JSMSG_NESTING_GENERATOR);

    try {
        return resumeGenerator(this, val, 'throw');
    } catch (e) {
        if (!StarGeneratorObjectIsClosed(this))
            GeneratorSetClosed(this);
        throw e;
    }
}

function LegacyGeneratorNext(val) {
    if (!IsObject(this) || !IsLegacyGeneratorObject(this))
        return callFunction(CallLegacyGeneratorMethodIfWrapped, this, val, "LegacyGeneratorNext");

    if (LegacyGeneratorObjectIsClosed(this))
        ThrowStopIteration();

    if (GeneratorIsRunning(this))
        ThrowError(JSMSG_NESTING_GENERATOR);

    try {
        return resumeGenerator(this, val, 'next');
    } catch(e) {
        if (!LegacyGeneratorObjectIsClosed(this))
            GeneratorSetClosed(this);
        throw e;
    }
}

function LegacyGeneratorThrow(val) {
    if (!IsObject(this) || !IsLegacyGeneratorObject(this))
        return callFunction(CallLegacyGeneratorMethodIfWrapped, this, val, "LegacyGeneratorThrow");

    if (LegacyGeneratorObjectIsClosed(this))
        throw val;

    if (GeneratorIsRunning(this))
        ThrowError(JSMSG_NESTING_GENERATOR);

    try {
        return resumeGenerator(this, val, 'throw');
    } catch(e) {
        if (!LegacyGeneratorObjectIsClosed(this))
            GeneratorSetClosed(this);
        throw e;
    }
}

// Called by js::CloseIterator.
function LegacyGeneratorCloseInternal() {
    assert(IsObject(this), "Not an object: " + ToString(this));
    assert(IsLegacyGeneratorObject(this), "Not a legacy generator object: " + ToString(this));
    assert(!LegacyGeneratorObjectIsClosed(this), "Already closed: " + ToString(this));
    assert(!CloseNewbornLegacyGeneratorObject(this), "Newborn: " + ToString(this));

    if (GeneratorIsRunning(this))
        ThrowError(JSMSG_NESTING_GENERATOR);

    resumeGenerator(this, undefined, 'close');
    if (!LegacyGeneratorObjectIsClosed(this))
        CloseClosingLegacyGeneratorObject(this);
}

function LegacyGeneratorClose() {
    if (!IsObject(this) || !IsLegacyGeneratorObject(this))
        return callFunction(CallLegacyGeneratorMethodIfWrapped, this, "LegacyGeneratorClose");

    if (LegacyGeneratorObjectIsClosed(this) || CloseNewbornLegacyGeneratorObject(this))
        return;

    callFunction(LegacyGeneratorCloseInternal, this);
}
