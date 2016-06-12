// Globals, to make testing and debugging easier.
var context;
var filter;
var signal;
var renderedBuffer;
var renderedData;

var sampleRate = 44100.0;
var pulseLengthFrames = .1 * sampleRate;

// Maximum allowed error for the test to succeed.  Experimentally determined.
var maxAllowedError = 5.9e-8;

// This must be large enough so that the filtered result is
// essentially zero.  See comments for createTestAndRun.
var timeStep = .1;

// Maximum number of filters we can process (mostly for setting the
// render length correctly.)
var maxFilters = 5;

// How long to render.  Must be long enough for all of the filters we
// want to test.
var renderLengthSeconds = timeStep * (maxFilters + 1) ;

var renderLengthSamples = Math.round(renderLengthSeconds * sampleRate);

// Number of filters that will be processed.
var nFilters;

function createImpulseBuffer(context, length) {
    var impulse = context.createBuffer(1, length, context.sampleRate);
    var data = impulse.getChannelData(0);
    for (var k = 1; k < data.length; ++k) {
        data[k] = 0;
    }
    data[0] = 1;

    return impulse;
}


function createTestAndRun(context, filterType, filterParameters) {
    // To test the filters, we apply a signal (an impulse) to each of
    // the specified filters, with each signal starting at a different
    // time.  The output of the filters is summed together at the
    // output.  Thus for filter k, the signal input to the filter
    // starts at time k * timeStep.  For this to work well, timeStep
    // must be large enough for the output of each filter to have
    // decayed to zero with timeStep seconds.  That way the filter
    // outputs don't interfere with each other.

    nFilters = Math.min(filterParameters.length, maxFilters);

    signal = new Array(nFilters);
    filter = new Array(nFilters);

    impulse = createImpulseBuffer(context, pulseLengthFrames);

    // Create all of the signal sources and filters that we need.
    for (var k = 0; k < nFilters; ++k) {
        signal[k] = context.createBufferSource();
        signal[k].buffer = impulse;

        filter[k] = context.createBiquadFilter();
        filter[k].type = filterType;
        filter[k].frequency.value = context.sampleRate / 2 * filterParameters[k].cutoff;
        filter[k].detune.value = (filterParameters[k].detune === undefined) ? 0 : filterParameters[k].detune;
        filter[k].Q.value = filterParameters[k].q;
        filter[k].gain.value = filterParameters[k].gain;

        signal[k].connect(filter[k]);
        filter[k].connect(context.destination);

        signal[k].start(timeStep * k);
    }

    context.oncomplete = checkFilterResponse(filterType, filterParameters);
    context.startRendering();
}

function addSignal(dest, src, destOffset) {
    // Add src to dest at the given dest offset.
    for (var k = destOffset, j = 0; k < dest.length, j < src.length; ++k, ++j) {
        dest[k] += src[j];
    }
}

function generateReference(filterType, filterParameters) {
    var result = new Array(renderLengthSamples);
    var data = new Array(renderLengthSamples);
    // Initialize the result array and data.
    for (var k = 0; k < result.length; ++k) {
        result[k] = 0;
        data[k] = 0;
    }
    // Make data an impulse.
    data[0] = 1;

    for (var k = 0; k < nFilters; ++k) {
        // Filter an impulse
        var detune = (filterParameters[k].detune === undefined) ? 0 : filterParameters[k].detune;
        var frequency = filterParameters[k].cutoff * Math.pow(2, detune / 1200); // Apply detune, converting from Cents.

        var filterCoef = createFilter(filterType,
                                      frequency,
                                      filterParameters[k].q,
                                      filterParameters[k].gain);
        var y = filterData(filterCoef, data, renderLengthSamples);

        // Accumulate this filtered data into the final output at the desired offset.
        addSignal(result, y, timeToSampleFrame(timeStep * k, sampleRate));
    }

    return result;
}

function checkFilterResponse(filterType, filterParameters) {
    return function(event) {
        renderedBuffer = event.renderedBuffer;
        renderedData = renderedBuffer.getChannelData(0);

        reference = generateReference(filterType, filterParameters);

        var len = Math.min(renderedData.length, reference.length);

        var success = true;

        // Maximum error between rendered data and expected data
        var maxError = 0;

        // Sample offset where the maximum error occurred.
        var maxPosition = 0;

        // Number of infinities or NaNs that occurred in the rendered data.
        var invalidNumberCount = 0;

        ok(nFilters == filterParameters.length, "Test wanted " + filterParameters.length + " filters but only " + maxFilters + " allowed.");

        compareChannels(renderedData, reference, len, 0, 0, true);

        // Check for bad numbers in the rendered output too.
        // There shouldn't be any.
        for (var k = 0; k < len; ++k) {
            if (!isValidNumber(renderedData[k])) {
                ++invalidNumberCount;
            }
        }

        ok(invalidNumberCount == 0, "Rendered output has " + invalidNumberCount + " infinities or NaNs.");
        SimpleTest.finish();
    }
}
