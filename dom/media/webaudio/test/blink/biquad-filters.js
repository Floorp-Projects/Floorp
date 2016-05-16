// Taken from WebKit/LayoutTests/webaudio/resources/biquad-filters.js

// A biquad filter has a z-transform of
// H(z) = (b0 + b1 / z + b2 / z^2) / (1 + a1 / z + a2 / z^2)
//
// The formulas for the various filters were taken from
// http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt.


// Lowpass filter.
function createLowpassFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;

    if (freq == 1) {
        // The formula below works, except for roundoff.  When freq = 1,
        // the filter is just a wire, so hardwire the coefficients.
        b0 = 1;
        b1 = 0;
        b2 = 0;
        a0 = 1;
        a1 = 0;
        a2 = 0;
    } else {
        var w0 = Math.PI * freq;
        var alpha = 0.5 * Math.sin(w0) / Math.pow(10, q / 20);
        var cos_w0 = Math.cos(w0);

        b0 = 0.5 * (1 - cos_w0);
        b1 = 1 - cos_w0;
        b2 = b0;
        a0 = 1 + alpha;
        a1 = -2.0 * cos_w0;
        a2 = 1 - alpha;
    }

    return normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
}

function createHighpassFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a1;
    var a2;

    if (freq == 1) {
        // The filter is 0
        b0 = 0;
        b1 = 0;
        b2 = 0;
        a0 = 1;
        a1 = 0;
        a2 = 0;
    } else if (freq == 0) {
        // The filter is 1.  Computation of coefficients below is ok, but
        // there's a pole at 1 and a zero at 1, so round-off could make
        // the filter unstable.
        b0 = 1;
        b1 = 0;
        b2 = 0;
        a0 = 1;
        a1 = 0;
        a2 = 0;
    } else {
        var w0 = Math.PI * freq;
        var alpha = 0.5 * Math.sin(w0) / Math.pow(10, q / 20);
        var cos_w0 = Math.cos(w0);

        b0 = 0.5 * (1 + cos_w0);
        b1 = -1 - cos_w0;
        b2 = b0;
        a0 = 1 + alpha;
        a1 = -2.0 * cos_w0;
        a2 = 1 - alpha;
    }

    return normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
}

function normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2) {
    var scale = 1 / a0;

    return {b0 : b0 * scale,
            b1 : b1 * scale,
            b2 : b2 * scale,
            a1 : a1 * scale,
            a2 : a2 * scale};
}

function createBandpassFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    if (freq > 0 && freq < 1) {
        var w0 = Math.PI * freq;
        if (q > 0) {
            var alpha = Math.sin(w0) / (2 * q);
            var k = Math.cos(w0);

            b0 = alpha;
            b1 = 0;
            b2 = -alpha;
            a0 = 1 + alpha;
            a1 = -2 * k;
            a2 = 1 - alpha;

            coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
        } else {
            // q = 0, and frequency is not 0 or 1.  The above formula has a
            // divide by zero problem.  The limit of the z-transform as q
            // approaches 0 is 1, so set the filter that way.
            coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
        }
    } else {
        // When freq = 0 or 1, the z-transform is identically 0,
        // independent of q.
        coef = {b0 : 0, b1 : 0, b2 : 0, a1 : 0, a2 : 0}
    }

    return coef;
}

function createLowShelfFilter(freq, q, gain) {
    // q not used
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    var S = 1;
    var A = Math.pow(10, gain / 40);

    if (freq == 1) {
        // The filter is just a constant gain
        coef = {b0 : A * A, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    } else if (freq == 0) {
        // The filter is 1
        coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    } else {
        var w0 = Math.PI * freq;
        var alpha = 1 / 2 * Math.sin(w0) * Math.sqrt((A + 1 / A) * (1 / S - 1) + 2);
        var k = Math.cos(w0);
        var k2 = 2 * Math.sqrt(A) * alpha;
        var Ap1 = A + 1;
        var Am1 = A - 1;

        b0 = A * (Ap1 - Am1 * k + k2);
        b1 = 2 * A * (Am1 - Ap1 * k);
        b2 = A * (Ap1 - Am1 * k - k2);
        a0 = Ap1 + Am1 * k + k2;
        a1 = -2 * (Am1 + Ap1 * k);
        a2 = Ap1 + Am1 * k - k2;
        coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
    }

    return coef;
}

function createHighShelfFilter(freq, q, gain) {
    // q not used
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    var A = Math.pow(10, gain / 40);

    if (freq == 1) {
        // When freq = 1, the z-transform is 1
        coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    } else if (freq > 0) {
        var w0 = Math.PI * freq;
        var S = 1;
        var alpha = 0.5 * Math.sin(w0) * Math.sqrt((A + 1 / A) * (1 / S - 1) + 2);
        var k = Math.cos(w0);
        var k2 = 2 * Math.sqrt(A) * alpha;
        var Ap1 = A + 1;
        var Am1 = A - 1;

        b0 = A * (Ap1 + Am1 * k + k2);
        b1 = -2 * A * (Am1 + Ap1 * k);
        b2 = A * (Ap1 + Am1 * k - k2);
        a0 = Ap1 - Am1 * k + k2;
        a1 = 2 * (Am1 - Ap1*k);
        a2 = Ap1 - Am1 * k-k2;

        coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
    } else {
        // When freq = 0, the filter is just a gain
        coef = {b0 : A * A, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    }

    return coef;
}

function createPeakingFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    var A = Math.pow(10, gain / 40);

    if (freq > 0 && freq < 1) {
        if (q > 0) {
            var w0 = Math.PI * freq;
            var alpha = Math.sin(w0) / (2 * q);
            var k = Math.cos(w0);

            b0 = 1 + alpha * A;
            b1 = -2 * k;
            b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;
            a1 = -2 * k;
            a2 = 1 - alpha / A;

            coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
        } else {
            // q = 0, we have a divide by zero problem in the formulas
            // above.  But if we look at the z-transform, we see that the
            // limit as q approaches 0 is A^2.
            coef = {b0 : A * A, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
        }
    } else {
        // freq = 0 or 1, the z-transform is 1
        coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    }

    return coef;
}

function createNotchFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    if (freq > 0 && freq < 1) {
        if (q > 0) {
            var w0 = Math.PI * freq;
            var alpha = Math.sin(w0) / (2 * q);
            var k = Math.cos(w0);

            b0 = 1;
            b1 = -2 * k;
            b2 = 1;
            a0 = 1 + alpha;
            a1 = -2 * k;
            a2 = 1 - alpha;
            coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
        } else {
            // When q = 0, we get a divide by zero above.  The limit of the
            // z-transform as q approaches 0 is 0, so set the coefficients
            // appropriately.
            coef = {b0 : 0, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
        }
    } else {
        // When freq = 0 or 1, the z-transform is 1
        coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    }

    return coef;
}

function createAllpassFilter(freq, q, gain) {
    var b0;
    var b1;
    var b2;
    var a0;
    var a1;
    var a2;
    var coef;

    if (freq > 0 && freq < 1) {
        if (q > 0) {
            var w0 = Math.PI * freq;
            var alpha = Math.sin(w0) / (2 * q);
            var k = Math.cos(w0);

            b0 = 1 - alpha;
            b1 = -2 * k;
            b2 = 1 + alpha;
            a0 = 1 + alpha;
            a1 = -2 * k;
            a2 = 1 - alpha;
            coef = normalizeFilterCoefficients(b0, b1, b2, a0, a1, a2);
        } else {
            // q = 0
            coef = {b0 : -1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
        }
    } else {
        coef = {b0 : 1, b1 : 0, b2 : 0, a1 : 0, a2 : 0};
    }

    return coef;
}

function filterData(filterCoef, signal, len) {
    var y = new Array(len);
    var b0 = filterCoef.b0;
    var b1 = filterCoef.b1;
    var b2 = filterCoef.b2;
    var a1 = filterCoef.a1;
    var a2 = filterCoef.a2;

    // Prime the pump. (Assumes the signal has length >= 2!)
    y[0] = b0 * signal[0];
    y[1] = b0 * signal[1] + b1 * signal[0] - a1 * y[0];

    // Filter all of the signal that we have.
    for (var k = 2; k < Math.min(signal.length, len); ++k) {
        y[k] = b0 * signal[k] + b1 * signal[k-1] + b2 * signal[k-2] - a1 * y[k-1] - a2 * y[k-2];
    }

    // If we need to filter more, but don't have any signal left,
    // assume the signal is zero.
    for (var k = signal.length; k < len; ++k) {
        y[k] = - a1 * y[k-1] - a2 * y[k-2];
    }

    return y;
}

// Map the filter type name to a function that computes the filter coefficents for the given filter
// type.
var filterCreatorFunction = {"lowpass": createLowpassFilter,
                             "highpass": createHighpassFilter,
                             "bandpass": createBandpassFilter,
                             "lowshelf": createLowShelfFilter,
                             "highshelf": createHighShelfFilter,
                             "peaking": createPeakingFilter,
                             "notch": createNotchFilter,
                             "allpass": createAllpassFilter};

var filterTypeName = {"lowpass": "Lowpass filter",
                      "highpass": "Highpass filter",
                      "bandpass": "Bandpass filter",
                      "lowshelf": "Lowshelf filter",
                      "highshelf": "Highshelf filter",
                      "peaking": "Peaking filter",
                      "notch": "Notch filter",
                      "allpass": "Allpass filter"};

function createFilter(filterType, freq, q, gain) {
    return filterCreatorFunction[filterType](freq, q, gain);
}
