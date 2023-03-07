/* codemirror-colorpicker version 1.9.72 */
(function (global, factory) {
	typeof exports === 'object' && typeof module !== 'undefined' ? module.exports = factory() :
	typeof define === 'function' && define.amd ? define(factory) :
	(global['codemirror-colorpicker'] = factory());
}(this || self, (function () { 'use strict';

/**
 * @method format
 *
 * convert color to format string
 *
 *     // hex
 *     color.format({ r : 255, g : 255, b : 255, a: 1 }, 'hex')  // #FFFFFFFF
 *
 *     // rgb
 *     color.format({ r : 255, g : 255, b : 255 }, 'rgb') // rgba(255, 255, 255, 0.5);
 *
 *     // rgba
 *     color.format({ r : 255, g : 255, b : 255, a : 0.5 }, 'rgb') // rgba(255, 255, 255, 0.5);
 *
 * @param {Object} obj  obj has r, g, b and a attributes
 * @param {"hex"/"rgb"} type  format string type
 * @returns {*}
 */
function format(obj, type) {
    var defaultColor = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 'rgba(0, 0, 0, 0)';


    if (Array.isArray(obj)) {
        obj = { r: obj[0], g: obj[1], b: obj[2], a: obj[3] };
    }

    if (type == 'hex') {
        return hex(obj);
    } else if (type == 'rgb') {
        return rgb(obj, defaultColor);
    } else if (type == 'hsl') {
        return hsl(obj);
    }

    return obj;
}

function hex(obj) {
    if (Array.isArray(obj)) {
        obj = { r: obj[0], g: obj[1], b: obj[2], a: obj[3] };
    }

    var r = obj.r.toString(16);
    if (obj.r < 16) r = "0" + r;

    var g = obj.g.toString(16);
    if (obj.g < 16) g = "0" + g;

    var b = obj.b.toString(16);
    if (obj.b < 16) b = "0" + b;

    var alphaValue = '';
    if (obj.a < 1) {
        var alpha = Math.floor(obj.a * 255);
        var alphaValue = alpha.toString(16);
        if (alpha < 16) alphaValue = "0" + alphaValue;
    }

    return '#' + r + g + b + alphaValue;
}

function rgb(obj) {
    var defaultColor = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'rgba(0, 0, 0, 0)';

    if (Array.isArray(obj)) {
        obj = { r: obj[0], g: obj[1], b: obj[2], a: obj[3] };
    }

    if (typeof obj == 'undefined') {
        return undefined;
    }

    if (obj.a == 1 || typeof obj.a == 'undefined') {
        if (isNaN(obj.r)) {
            return defaultColor;
        }
        return 'rgb(' + obj.r + ',' + obj.g + ',' + obj.b + ')';
    } else {
        return 'rgba(' + obj.r + ',' + obj.g + ',' + obj.b + ',' + obj.a + ')';
    }
}

function hsl(obj) {
    if (Array.isArray(obj)) {
        obj = { r: obj[0], g: obj[1], b: obj[2], a: obj[3] };
    }

    if (obj.a == 1 || typeof obj.a == 'undefined') {
        return 'hsl(' + obj.h + ',' + obj.s + '%,' + obj.l + '%)';
    } else {
        return 'hsla(' + obj.h + ',' + obj.s + '%,' + obj.l + '%,' + obj.a + ')';
    }
}

var formatter = {
    format: format,
    rgb: rgb,
    hsl: hsl,
    hex: hex
};

function round(n, k) {
    k = typeof k == 'undefined' ? 1 : k;
    return Math.round(n * k) / k;
}

function degreeToRadian(angle) {
    return angle * Math.PI / 180;
}

/**
 * 
 * convert radian to degree 
 * 
 * @param {*} radian 
 * @returns {Number} 0..360
 */
function radianToDegree(radian) {
    var angle = radian * 180 / Math.PI;

    if (angle < 0) {
        // 각도가 0보다 작으면 360 에서 반전시킨다. 
        angle = 360 + angle;
    }

    return angle;
}

function getXInCircle(angle, radius) {
    var centerX = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0;

    return centerX + radius * Math.cos(degreeToRadian(angle));
}

function getYInCircle(angle, radius) {
    var centerY = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0;

    return centerY + radius * Math.sin(degreeToRadian(angle));
}

function getXYInCircle(angle, radius) {
    var centerX = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0;
    var centerY = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 0;

    return {
        x: getXInCircle(angle, radius, centerX),
        y: getYInCircle(angle, radius, centerY)
    };
}

function caculateAngle(rx, ry) {
    return radianToDegree(Math.atan2(ry, rx));
}

var math = {
    round: round,
    radianToDegree: radianToDegree,
    degreeToRadian: degreeToRadian,
    getXInCircle: getXInCircle,
    getYInCircle: getYInCircle,
    caculateAngle: caculateAngle
};

var color_names = { aliceblue: "rgb(240, 248, 255)", antiquewhite: "rgb(250, 235, 215)", aqua: "rgb(0, 255, 255)", aquamarine: "rgb(127, 255, 212)", azure: "rgb(240, 255, 255)", beige: "rgb(245, 245, 220)", bisque: "rgb(255, 228, 196)", black: "rgb(0, 0, 0)", blanchedalmond: "rgb(255, 235, 205)", blue: "rgb(0, 0, 255)", blueviolet: "rgb(138, 43, 226)", brown: "rgb(165, 42, 42)", burlywood: "rgb(222, 184, 135)", cadetblue: "rgb(95, 158, 160)", chartreuse: "rgb(127, 255, 0)", chocolate: "rgb(210, 105, 30)", coral: "rgb(255, 127, 80)", cornflowerblue: "rgb(100, 149, 237)", cornsilk: "rgb(255, 248, 220)", crimson: "rgb(237, 20, 61)", cyan: "rgb(0, 255, 255)", darkblue: "rgb(0, 0, 139)", darkcyan: "rgb(0, 139, 139)", darkgoldenrod: "rgb(184, 134, 11)", darkgray: "rgb(169, 169, 169)", darkgrey: "rgb(169, 169, 169)", darkgreen: "rgb(0, 100, 0)", darkkhaki: "rgb(189, 183, 107)", darkmagenta: "rgb(139, 0, 139)", darkolivegreen: "rgb(85, 107, 47)", darkorange: "rgb(255, 140, 0)", darkorchid: "rgb(153, 50, 204)", darkred: "rgb(139, 0, 0)", darksalmon: "rgb(233, 150, 122)", darkseagreen: "rgb(143, 188, 143)", darkslateblue: "rgb(72, 61, 139)", darkslategray: "rgb(47, 79, 79)", darkslategrey: "rgb(47, 79, 79)", darkturquoise: "rgb(0, 206, 209)", darkviolet: "rgb(148, 0, 211)", deeppink: "rgb(255, 20, 147)", deepskyblue: "rgb(0, 191, 255)", dimgray: "rgb(105, 105, 105)", dimgrey: "rgb(105, 105, 105)", dodgerblue: "rgb(30, 144, 255)", firebrick: "rgb(178, 34, 34)", floralwhite: "rgb(255, 250, 240)", forestgreen: "rgb(34, 139, 34)", fuchsia: "rgb(255, 0, 255)", gainsboro: "rgb(220, 220, 220)", ghostwhite: "rgb(248, 248, 255)", gold: "rgb(255, 215, 0)", goldenrod: "rgb(218, 165, 32)", gray: "rgb(128, 128, 128)", grey: "rgb(128, 128, 128)", green: "rgb(0, 128, 0)", greenyellow: "rgb(173, 255, 47)", honeydew: "rgb(240, 255, 240)", hotpink: "rgb(255, 105, 180)", indianred: "rgb(205, 92, 92)", indigo: "rgb(75, 0, 130)", ivory: "rgb(255, 255, 240)", khaki: "rgb(240, 230, 140)", lavender: "rgb(230, 230, 250)", lavenderblush: "rgb(255, 240, 245)", lawngreen: "rgb(124, 252, 0)", lemonchiffon: "rgb(255, 250, 205)", lightblue: "rgb(173, 216, 230)", lightcoral: "rgb(240, 128, 128)", lightcyan: "rgb(224, 255, 255)", lightgoldenrodyellow: "rgb(250, 250, 210)", lightgreen: "rgb(144, 238, 144)", lightgray: "rgb(211, 211, 211)", lightgrey: "rgb(211, 211, 211)", lightpink: "rgb(255, 182, 193)", lightsalmon: "rgb(255, 160, 122)", lightseagreen: "rgb(32, 178, 170)", lightskyblue: "rgb(135, 206, 250)", lightslategray: "rgb(119, 136, 153)", lightslategrey: "rgb(119, 136, 153)", lightsteelblue: "rgb(176, 196, 222)", lightyellow: "rgb(255, 255, 224)", lime: "rgb(0, 255, 0)", limegreen: "rgb(50, 205, 50)", linen: "rgb(250, 240, 230)", magenta: "rgb(255, 0, 255)", maroon: "rgb(128, 0, 0)", mediumaquamarine: "rgb(102, 205, 170)", mediumblue: "rgb(0, 0, 205)", mediumorchid: "rgb(186, 85, 211)", mediumpurple: "rgb(147, 112, 219)", mediumseagreen: "rgb(60, 179, 113)", mediumslateblue: "rgb(123, 104, 238)", mediumspringgreen: "rgb(0, 250, 154)", mediumturquoise: "rgb(72, 209, 204)", mediumvioletred: "rgb(199, 21, 133)", midnightblue: "rgb(25, 25, 112)", mintcream: "rgb(245, 255, 250)", mistyrose: "rgb(255, 228, 225)", moccasin: "rgb(255, 228, 181)", navajowhite: "rgb(255, 222, 173)", navy: "rgb(0, 0, 128)", oldlace: "rgb(253, 245, 230)", olive: "rgb(128, 128, 0)", olivedrab: "rgb(107, 142, 35)", orange: "rgb(255, 165, 0)", orangered: "rgb(255, 69, 0)", orchid: "rgb(218, 112, 214)", palegoldenrod: "rgb(238, 232, 170)", palegreen: "rgb(152, 251, 152)", paleturquoise: "rgb(175, 238, 238)", palevioletred: "rgb(219, 112, 147)", papayawhip: "rgb(255, 239, 213)", peachpuff: "rgb(255, 218, 185)", peru: "rgb(205, 133, 63)", pink: "rgb(255, 192, 203)", plum: "rgb(221, 160, 221)", powderblue: "rgb(176, 224, 230)", purple: "rgb(128, 0, 128)", rebeccapurple: "rgb(102, 51, 153)", red: "rgb(255, 0, 0)", rosybrown: "rgb(188, 143, 143)", royalblue: "rgb(65, 105, 225)", saddlebrown: "rgb(139, 69, 19)", salmon: "rgb(250, 128, 114)", sandybrown: "rgb(244, 164, 96)", seagreen: "rgb(46, 139, 87)", seashell: "rgb(255, 245, 238)", sienna: "rgb(160, 82, 45)", silver: "rgb(192, 192, 192)", skyblue: "rgb(135, 206, 235)", slateblue: "rgb(106, 90, 205)", slategray: "rgb(112, 128, 144)", slategrey: "rgb(112, 128, 144)", snow: "rgb(255, 250, 250)", springgreen: "rgb(0, 255, 127)", steelblue: "rgb(70, 130, 180)", tan: "rgb(210, 180, 140)", teal: "rgb(0, 128, 128)", thistle: "rgb(216, 191, 216)", tomato: "rgb(255, 99, 71)", turquoise: "rgb(64, 224, 208)", violet: "rgb(238, 130, 238)", wheat: "rgb(245, 222, 179)", white: "rgb(255, 255, 255)", whitesmoke: "rgb(245, 245, 245)", yellow: "rgb(255, 255, 0)", yellowgreen: "rgb(154, 205, 50)", transparent: "rgba(0, 0, 0, 0)" };

function isColorName(name) {
    return !!color_names[name];
}

function getColorByName(name) {
    return color_names[name];
}

var ColorNames = {
    isColorName: isColorName,
    getColorByName: getColorByName
};

function HUEtoRGB(p, q, t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1 / 6) return p + (q - p) * 6 * t;
    if (t < 1 / 2) return q;
    if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
    return p;
}

function HSLtoHSV(h, s, l) {

    if (arguments.length == 1) {
        var _arguments$ = arguments[0],
            h = _arguments$.h,
            s = _arguments$.s,
            l = _arguments$.l;
    }
    var rgb = HSLtoRGB(h, s, l);

    return RGBtoHSV(rgb.r, rgb.g, rgb.b);
}

function HSLtoRGB(h, s, l) {

    if (arguments.length == 1) {
        var _arguments$2 = arguments[0],
            h = _arguments$2.h,
            s = _arguments$2.s,
            l = _arguments$2.l;
    }

    var r, g, b;

    h /= 360;
    s /= 100;
    l /= 100;

    if (s == 0) {
        r = g = b = l; // achromatic
    } else {
        var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        var p = 2 * l - q;
        r = HUEtoRGB(p, q, h + 1 / 3);
        g = HUEtoRGB(p, q, h);
        b = HUEtoRGB(p, q, h - 1 / 3);
    }

    return { r: round(r * 255), g: round(g * 255), b: round(b * 255) };
}

var fromHSL = {
    HUEtoRGB: HUEtoRGB,
    HSLtoHSV: HSLtoHSV,
    HSLtoRGB: HSLtoRGB
};

var classCallCheck = function (instance, Constructor) {
  if (!(instance instanceof Constructor)) {
    throw new TypeError("Cannot call a class as a function");
  }
};

var createClass = function () {
  function defineProperties(target, props) {
    for (var i = 0; i < props.length; i++) {
      var descriptor = props[i];
      descriptor.enumerable = descriptor.enumerable || false;
      descriptor.configurable = true;
      if ("value" in descriptor) descriptor.writable = true;
      Object.defineProperty(target, descriptor.key, descriptor);
    }
  }

  return function (Constructor, protoProps, staticProps) {
    if (protoProps) defineProperties(Constructor.prototype, protoProps);
    if (staticProps) defineProperties(Constructor, staticProps);
    return Constructor;
  };
}();





var defineProperty = function (obj, key, value) {
  if (key in obj) {
    Object.defineProperty(obj, key, {
      value: value,
      enumerable: true,
      configurable: true,
      writable: true
    });
  } else {
    obj[key] = value;
  }

  return obj;
};

var _extends = Object.assign || function (target) {
  for (var i = 1; i < arguments.length; i++) {
    var source = arguments[i];

    for (var key in source) {
      if (Object.prototype.hasOwnProperty.call(source, key)) {
        target[key] = source[key];
      }
    }
  }

  return target;
};

var get = function get(object, property, receiver) {
  if (object === null) object = Function.prototype;
  var desc = Object.getOwnPropertyDescriptor(object, property);

  if (desc === undefined) {
    var parent = Object.getPrototypeOf(object);

    if (parent === null) {
      return undefined;
    } else {
      return get(parent, property, receiver);
    }
  } else if ("value" in desc) {
    return desc.value;
  } else {
    var getter = desc.get;

    if (getter === undefined) {
      return undefined;
    }

    return getter.call(receiver);
  }
};

var inherits = function (subClass, superClass) {
  if (typeof superClass !== "function" && superClass !== null) {
    throw new TypeError("Super expression must either be null or a function, not " + typeof superClass);
  }

  subClass.prototype = Object.create(superClass && superClass.prototype, {
    constructor: {
      value: subClass,
      enumerable: false,
      writable: true,
      configurable: true
    }
  });
  if (superClass) Object.setPrototypeOf ? Object.setPrototypeOf(subClass, superClass) : subClass.__proto__ = superClass;
};











var possibleConstructorReturn = function (self, call) {
  if (!self) {
    throw new ReferenceError("this hasn't been initialised - super() hasn't been called");
  }

  return call && (typeof call === "object" || typeof call === "function") ? call : self;
};





var slicedToArray = function () {
  function sliceIterator(arr, i) {
    var _arr = [];
    var _n = true;
    var _d = false;
    var _e = undefined;

    try {
      for (var _i = arr[Symbol.iterator](), _s; !(_n = (_s = _i.next()).done); _n = true) {
        _arr.push(_s.value);

        if (i && _arr.length === i) break;
      }
    } catch (err) {
      _d = true;
      _e = err;
    } finally {
      try {
        if (!_n && _i["return"]) _i["return"]();
      } finally {
        if (_d) throw _e;
      }
    }

    return _arr;
  }

  return function (arr, i) {
    if (Array.isArray(arr)) {
      return arr;
    } else if (Symbol.iterator in Object(arr)) {
      return sliceIterator(arr, i);
    } else {
      throw new TypeError("Invalid attempt to destructure non-iterable instance");
    }
  };
}();











var toArray = function (arr) {
  return Array.isArray(arr) ? arr : Array.from(arr);
};

var toConsumableArray = function (arr) {
  if (Array.isArray(arr)) {
    for (var i = 0, arr2 = Array(arr.length); i < arr.length; i++) arr2[i] = arr[i];

    return arr2;
  } else {
    return Array.from(arr);
  }
};

var color_regexp = /(#(?:[\da-f]{8})|#(?:[\da-f]{3}){1,2}|rgb\((?:\s*\d{1,3},\s*){2}\d{1,3}\s*\)|rgba\((?:\s*\d{1,3},\s*){3}\d*\.?\d+\s*\)|hsl\(\s*\d{1,3}(?:,\s*\d{1,3}%){2}\s*\)|hsla\(\s*\d{1,3}(?:,\s*\d{1,3}%){2},\s*\d*\.?\d+\s*\)|([\w_\-]+))/gi;
var color_split = ',';

function matches(str) {
    var matches = str.match(color_regexp);
    var result = [];

    if (!matches) {
        return result;
    }

    for (var i = 0, len = matches.length; i < len; i++) {

        if (matches[i].indexOf('#') > -1 || matches[i].indexOf('rgb') > -1 || matches[i].indexOf('hsl') > -1) {
            result.push({ color: matches[i] });
        } else {
            var nameColor = ColorNames.getColorByName(matches[i]);

            if (nameColor) {
                result.push({ color: matches[i], nameColor: nameColor });
            }
        }
    }

    var pos = { next: 0 };
    result.forEach(function (item) {
        var startIndex = str.indexOf(item.color, pos.next);

        item.startIndex = startIndex;
        item.endIndex = startIndex + item.color.length;

        pos.next = item.endIndex;
    });

    return result;
}

function convertMatches(str) {
    var m = matches(str);

    m.forEach(function (it, index) {
        str = str.replace(it.color, '@' + index);
    });

    return { str: str, matches: m };
}

function convertMatchesArray(str) {
    var splitStr = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : ',';

    var ret = convertMatches(str);
    return ret.str.split(splitStr).map(function (it, index) {
        it = trim(it);

        if (ret.matches[index]) {
            it = it.replace('@' + index, ret.matches[index].color);
        }

        return it;
    });
}

function reverseMatches(str, matches) {
    matches.forEach(function (it, index) {
        str = str.replace('@' + index, it.color);
    });

    return str;
}

function trim(str) {
    return str.replace(/^\s+|\s+$/g, '');
}

/**
 * @method rgb
 *
 * parse string to rgb color
 *
 * 		color.parse("#FF0000") === { r : 255, g : 0, b : 0 }
 *
 * 		color.parse("rgb(255, 0, 0)") == { r : 255, g : 0, b :0 }
 * 		color.parse(0xff0000) == { r : 255, g : 0, b : 0 }
 * 		color.parse(0xff000000) == { r : 255, g : 0, b : 0, a: 0 }
 *
 * @param {String} str color string
 * @returns {Object}  rgb object
 */
function parse(str) {
    if (typeof str == 'string') {

        if (ColorNames.isColorName(str)) {
            str = ColorNames.getColorByName(str);
        }

        if (str.indexOf("rgb(") > -1) {
            var arr = str.replace("rgb(", "").replace(")", "").split(",");

            for (var i = 0, len = arr.length; i < len; i++) {
                arr[i] = parseInt(trim(arr[i]), 10);
            }

            var obj = { type: 'rgb', r: arr[0], g: arr[1], b: arr[2], a: 1 };

            obj = Object.assign(obj, RGBtoHSL(obj));

            return obj;
        } else if (str.indexOf("rgba(") > -1) {
            var arr = str.replace("rgba(", "").replace(")", "").split(",");

            for (var i = 0, len = arr.length; i < len; i++) {

                if (len - 1 == i) {
                    arr[i] = parseFloat(trim(arr[i]));
                } else {
                    arr[i] = parseInt(trim(arr[i]), 10);
                }
            }

            var obj = { type: 'rgb', r: arr[0], g: arr[1], b: arr[2], a: arr[3] };

            obj = Object.assign(obj, RGBtoHSL(obj));

            return obj;
        } else if (str.indexOf("hsl(") > -1) {
            var arr = str.replace("hsl(", "").replace(")", "").split(",");

            for (var i = 0, len = arr.length; i < len; i++) {
                arr[i] = parseFloat(trim(arr[i]));
            }

            var obj = { type: 'hsl', h: arr[0], s: arr[1], l: arr[2], a: 1 };

            obj = Object.assign(obj, HSLtoRGB(obj));

            return obj;
        } else if (str.indexOf("hsla(") > -1) {
            var arr = str.replace("hsla(", "").replace(")", "").split(",");

            for (var i = 0, len = arr.length; i < len; i++) {

                if (len - 1 == i) {
                    arr[i] = parseFloat(trim(arr[i]));
                } else {
                    arr[i] = parseInt(trim(arr[i]), 10);
                }
            }

            var obj = { type: 'hsl', h: arr[0], s: arr[1], l: arr[2], a: arr[3] };

            obj = Object.assign(obj, HSLtoRGB(obj));

            return obj;
        } else if (str.indexOf("#") == 0) {

            str = str.replace("#", "");

            var arr = [];
            var a = 1;
            if (str.length == 3) {
                for (var i = 0, len = str.length; i < len; i++) {
                    var char = str.substr(i, 1);
                    arr.push(parseInt(char + char, 16));
                }
            } else if (str.length === 8) {
                for (var i = 0, len = str.length; i < len; i += 2) {
                    arr.push(parseInt(str.substr(i, 2), 16));
                }

                a = arr.pop() / 255;
            } else {
                for (var i = 0, len = str.length; i < len; i += 2) {
                    arr.push(parseInt(str.substr(i, 2), 16));
                }
            }

            var obj = { type: 'hex', r: arr[0], g: arr[1], b: arr[2], a: a };

            obj = Object.assign(obj, RGBtoHSL(obj));

            return obj;
        }
    } else if (typeof str == 'number') {
        if (0x000000 <= str && str <= 0xffffff) {
            var r = (str & 0xff0000) >> 16;
            var g = (str & 0x00ff00) >> 8;
            var b = (str & 0x0000ff) >> 0;

            var obj = { type: 'hex', r: r, g: g, b: b, a: 1 };
            obj = Object.assign(obj, RGBtoHSL(obj));
            return obj;
        } else if (0x00000000 <= str && str <= 0xffffffff) {
            var _r = (str & 0xff000000) >> 24;
            var _g = (str & 0x00ff0000) >> 16;
            var _b = (str & 0x0000ff00) >> 8;
            var _a = (str & 0x000000ff) / 255;

            var obj = { type: 'hex', r: _r, g: _g, b: _b, a: _a };
            obj = Object.assign(obj, RGBtoHSL(obj));

            return obj;
        }
    }

    return str;
}

function parseGradient(colors) {
    if (typeof colors == 'string') {
        colors = convertMatchesArray(colors);
    }

    colors = colors.map(function (it) {
        if (typeof it == 'string') {
            var ret = convertMatches(it);
            var arr = trim(ret.str).split(' ');

            if (arr[1]) {
                if (arr[1].includes('%')) {
                    arr[1] = parseFloat(arr[1].replace(/%/, '')) / 100;
                } else {
                    arr[1] = parseFloat(arr[1]);
                }
            } else {
                arr[1] = '*';
            }

            arr[0] = reverseMatches(arr[0], ret.matches);

            return arr;
        } else if (Array.isArray(it)) {

            if (!it[1]) {
                it[1] = '*';
            } else if (typeof it[1] == 'string') {
                if (it[1].includes('%')) {
                    it[1] = parseFloat(it[1].replace(/%/, '')) / 100;
                } else {
                    it[1] = +it[1];
                }
            }

            return [].concat(toConsumableArray(it));
        }
    });

    var count = colors.filter(function (it) {
        return it[1] === '*';
    }).length;

    if (count > 0) {
        var sum = colors.filter(function (it) {
            return it[1] != '*' && it[1] != 1;
        }).map(function (it) {
            return it[1];
        }).reduce(function (total, cur) {
            return total + cur;
        }, 0);

        var dist = (1 - sum) / count;
        colors.forEach(function (it, index) {
            if (it[1] == '*' && index > 0) {
                if (colors.length - 1 == index) {
                    // it[1] = 1 
                } else {
                    it[1] = dist;
                }
            }
        });
    }

    return colors;
}

var parser = {
    matches: matches,
    convertMatches: convertMatches,
    convertMatchesArray: convertMatchesArray,
    reverseMatches: reverseMatches,
    parse: parse,
    parseGradient: parseGradient,
    trim: trim,
    color_regexp: color_regexp,
    color_split: color_split
};

/**
 * @method RGBtoHSV
 *
 * convert rgb to hsv
 *
 * 		color.RGBtoHSV(0, 0, 255) === { h : 240, s : 1, v : 1 } === '#FFFF00'
 *
 * @param {Number} R  red color value
 * @param {Number} G  green color value
 * @param {Number} B  blue color value
 * @return {Object}  hsv color code
 */
function RGBtoHSV(r, g, b) {

    if (arguments.length == 1) {
        var _arguments$ = arguments[0],
            r = _arguments$.r,
            g = _arguments$.g,
            b = _arguments$.b;
    }

    var R1 = r / 255;
    var G1 = g / 255;
    var B1 = b / 255;

    var MaxC = Math.max(R1, G1, B1);
    var MinC = Math.min(R1, G1, B1);

    var DeltaC = MaxC - MinC;

    var H = 0;

    if (DeltaC == 0) {
        H = 0;
    } else if (MaxC == R1) {
        H = 60 * ((G1 - B1) / DeltaC % 6);
    } else if (MaxC == G1) {
        H = 60 * ((B1 - R1) / DeltaC + 2);
    } else if (MaxC == B1) {
        H = 60 * ((R1 - G1) / DeltaC + 4);
    }

    if (H < 0) {
        H = 360 + H;
    }

    var S = 0;

    if (MaxC == 0) S = 0;else S = DeltaC / MaxC;

    var V = MaxC;

    return { h: H, s: S, v: V };
}

function RGBtoCMYK(r, g, b) {

    if (arguments.length == 1) {
        var _arguments$2 = arguments[0],
            r = _arguments$2.r,
            g = _arguments$2.g,
            b = _arguments$2.b;
    }

    var R1 = r / 255;
    var G1 = g / 255;
    var B1 = b / 255;

    var K = 1 - Math.max(R1, G1, B1);
    var C = (1 - R1 - K) / (1 - K);
    var M = (1 - G1 - K) / (1 - K);
    var Y = (1 - B1 - K) / (1 - K);

    return { c: C, m: M, y: Y, k: K };
}

function RGBtoHSL(r, g, b) {

    if (arguments.length == 1) {
        var _arguments$3 = arguments[0],
            r = _arguments$3.r,
            g = _arguments$3.g,
            b = _arguments$3.b;
    }

    r /= 255, g /= 255, b /= 255;
    var max = Math.max(r, g, b),
        min = Math.min(r, g, b);
    var h,
        s,
        l = (max + min) / 2;

    if (max == min) {
        h = s = 0; // achromatic
    } else {
        var d = max - min;
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
        switch (max) {
            case r:
                h = (g - b) / d + (g < b ? 6 : 0);break;
            case g:
                h = (b - r) / d + 2;break;
            case b:
                h = (r - g) / d + 4;break;
        }
        h /= 6;
    }

    return { h: round(h * 360), s: round(s * 100), l: round(l * 100) };
}

function c(r, g, b) {

    if (arguments.length == 1) {
        var _arguments$4 = arguments[0],
            r = _arguments$4.r,
            g = _arguments$4.g,
            b = _arguments$4.b;
    }
    return gray((r + g + b) / 3 > 90 ? 0 : 255);
}

function gray(gray) {
    return { r: gray, g: gray, b: gray };
}

function RGBtoSimpleGray(r, g, b) {

    if (arguments.length == 1) {
        var _arguments$5 = arguments[0],
            r = _arguments$5.r,
            g = _arguments$5.g,
            b = _arguments$5.b;
    }
    return gray(Math.ceil((r + g + b) / 3));
}

function RGBtoGray(r, g, b) {

    if (arguments.length == 1) {
        var _arguments$6 = arguments[0],
            r = _arguments$6.r,
            g = _arguments$6.g,
            b = _arguments$6.b;
    }
    return gray(RGBtoYCrCb(r, g, b).y);
}

function brightness(r, g, b) {
    return Math.ceil(r * 0.2126 + g * 0.7152 + b * 0.0722);
}







function RGBtoYCrCb(r, g, b) {

    if (arguments.length == 1) {
        var _arguments$7 = arguments[0],
            r = _arguments$7.r,
            g = _arguments$7.g,
            b = _arguments$7.b;
    }
    var Y = brightness(r, g, b);
    var Cb = 0.564 * (b - Y);
    var Cr = 0.713 * (r - Y);

    return { y: Y, cr: Cr, cb: Cb };
}

function PivotRGB(n) {
    var point = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0.04045;

    return (n > point ? Math.pow((n + 0.055) / 1.055, 2.4) : n / 12.92) * 100;
}

function RGBtoXYZ(r, g, b) {
    //sR, sG and sB (Standard RGB) input range = 0 ÷ 255
    //X, Y and Z output refer to a D65/2° standard illuminant.
    if (arguments.length == 1) {
        var _arguments$8 = arguments[0],
            r = _arguments$8.r,
            g = _arguments$8.g,
            b = _arguments$8.b;
    }

    var R = r / 255;
    var G = g / 255;
    var B = b / 255;

    R = PivotRGB(R);
    G = PivotRGB(G);
    B = PivotRGB(B);

    var x = R * 0.4124 + G * 0.3576 + B * 0.1805;
    var y = R * 0.2126 + G * 0.7152 + B * 0.0722;
    var z = R * 0.0193 + G * 0.1192 + B * 0.9505;

    return { x: x, y: y, z: z };
}

function RGBtoLAB(r, g, b) {
    if (arguments.length == 1) {
        var _arguments$9 = arguments[0],
            r = _arguments$9.r,
            g = _arguments$9.g,
            b = _arguments$9.b;
    }
    return XYZtoLAB(RGBtoXYZ(r, g, b));
}

var fromRGB = {
    RGBtoCMYK: RGBtoCMYK,
    RGBtoGray: RGBtoGray,
    RGBtoHSL: RGBtoHSL,
    RGBtoHSV: RGBtoHSV,
    RGBtoLAB: RGBtoLAB,
    RGBtoSimpleGray: RGBtoSimpleGray,
    RGBtoXYZ: RGBtoXYZ,
    RGBtoYCrCb: RGBtoYCrCb,
    c: c,
    brightness: brightness,
    gray: gray
};

function CMYKtoRGB(c, m, y, k) {

    if (arguments.length == 1) {
        var _arguments$ = arguments[0],
            c = _arguments$.c,
            m = _arguments$.m,
            y = _arguments$.y,
            k = _arguments$.k;
    }

    var R = 255 * (1 - c) * (1 - k);
    var G = 255 * (1 - m) * (1 - k);
    var B = 255 * (1 - y) * (1 - k);

    return { r: R, g: G, b: B };
}

var fromCMYK = {
    CMYKtoRGB: CMYKtoRGB
};

function ReverseXyz(n) {
    return Math.pow(n, 3) > 0.008856 ? Math.pow(n, 3) : (n - 16 / 116) / 7.787;
}

function ReverseRGB(n) {
    return n > 0.0031308 ? 1.055 * Math.pow(n, 1 / 2.4) - 0.055 : 12.92 * n;
}

function XYZtoRGB(x, y, z) {
    if (arguments.length == 1) {
        var _arguments$ = arguments[0],
            x = _arguments$.x,
            y = _arguments$.y,
            z = _arguments$.z;
    }
    //X, Y and Z input refer to a D65/2° standard illuminant.
    //sR, sG and sB (standard RGB) output range = 0 ÷ 255

    var X = x / 100.0;
    var Y = y / 100.0;
    var Z = z / 100.0;

    var R = X * 3.2406 + Y * -1.5372 + Z * -0.4986;
    var G = X * -0.9689 + Y * 1.8758 + Z * 0.0415;
    var B = X * 0.0557 + Y * -0.2040 + Z * 1.0570;

    R = ReverseRGB(R);
    G = ReverseRGB(G);
    B = ReverseRGB(B);

    var r = round(R * 255);
    var g = round(G * 255);
    var b = round(B * 255);

    return { r: r, g: g, b: b };
}

function LABtoXYZ(l, a, b) {
    if (arguments.length == 1) {
        var _arguments$2 = arguments[0],
            l = _arguments$2.l,
            a = _arguments$2.a,
            b = _arguments$2.b;
    }
    //Reference-X, Y and Z refer to specific illuminants and observers.
    //Common reference values are available below in this same page.

    var Y = (l + 16) / 116;
    var X = a / 500 + Y;
    var Z = Y - b / 200;

    Y = ReverseXyz(Y);
    X = ReverseXyz(X);
    Z = ReverseXyz(Z);

    var x = X * 95.047;
    var y = Y * 100.000;
    var z = Z * 108.883;

    return { x: x, y: y, z: z };
}





function LABtoRGB(l, a, b) {
    if (arguments.length == 1) {
        var _arguments$4 = arguments[0],
            l = _arguments$4.l,
            a = _arguments$4.a,
            b = _arguments$4.b;
    }
    return XYZtoRGB(LABtoXYZ(l, a, b));
}

var fromLAB = {
    XYZtoRGB: XYZtoRGB,
    LABtoRGB: LABtoRGB,
    LABtoXYZ: LABtoXYZ
};

/**
 * @method HSVtoRGB
 *
 * convert hsv to rgb
 *
 * 		color.HSVtoRGB(0,0,1) === #FFFFF === { r : 255, g : 0, b : 0 }
 *
 * @param {Number} H  hue color number  (min : 0, max : 360)
 * @param {Number} S  Saturation number  (min : 0, max : 1)
 * @param {Number} V  Value number 		(min : 0, max : 1 )
 * @returns {Object}
 */
function HSVtoRGB(h, s, v) {

    if (arguments.length == 1) {
        var _arguments$ = arguments[0],
            h = _arguments$.h,
            s = _arguments$.s,
            v = _arguments$.v;
    }

    var H = h;
    var S = s;
    var V = v;

    if (H >= 360) {
        H = 0;
    }

    var C = S * V;
    var X = C * (1 - Math.abs(H / 60 % 2 - 1));
    var m = V - C;

    var temp = [];

    if (0 <= H && H < 60) {
        temp = [C, X, 0];
    } else if (60 <= H && H < 120) {
        temp = [X, C, 0];
    } else if (120 <= H && H < 180) {
        temp = [0, C, X];
    } else if (180 <= H && H < 240) {
        temp = [0, X, C];
    } else if (240 <= H && H < 300) {
        temp = [X, 0, C];
    } else if (300 <= H && H < 360) {
        temp = [C, 0, X];
    }

    return {
        r: round((temp[0] + m) * 255),
        g: round((temp[1] + m) * 255),
        b: round((temp[2] + m) * 255)
    };
}

function HSVtoHSL(h, s, v) {

    if (arguments.length == 1) {
        var _arguments$2 = arguments[0],
            h = _arguments$2.h,
            s = _arguments$2.s,
            v = _arguments$2.v;
    }

    var rgb = HSVtoRGB(h, s, v);

    return RGBtoHSL(rgb.r, rgb.g, rgb.b);
}

var fromHSV = {
    HSVtoHSL: HSVtoHSL,
    HSVtoRGB: HSVtoRGB
};

function YCrCbtoRGB(y, cr, cb, bit) {

    if (arguments.length == 1) {
        var _arguments$ = arguments[0],
            y = _arguments$.y,
            cr = _arguments$.cr,
            cb = _arguments$.cb,
            bit = _arguments$.bit;

        bit = bit || 0;
    }
    var R = y + 1.402 * (cr - bit);
    var G = y - 0.344 * (cb - bit) - 0.714 * (cr - bit);
    var B = y + 1.772 * (cb - bit);

    return { r: Math.ceil(R), g: Math.ceil(G), b: Math.ceil(B) };
}

var fromYCrCb = {
    YCrCbtoRGB: YCrCbtoRGB
};

/**
 * @deprecated 
 * 
 * instead of this,  use blend function 
 *  
 * @param {*} startColor 
 * @param {*} endColor 
 * @param {*} t 
 */
function interpolateRGB(startColor, endColor) {
    var t = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0.5;
    var exportFormat = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 'hex';

    var obj = {
        r: round(startColor.r + (endColor.r - startColor.r) * t),
        g: round(startColor.g + (endColor.g - startColor.g) * t),
        b: round(startColor.b + (endColor.b - startColor.b) * t),
        a: round(startColor.a + (endColor.a - startColor.a) * t, 100)
    };

    return format(obj, obj.a < 1 ? 'rgb' : exportFormat);
}

function scale(scale) {
    var count = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 5;

    if (!scale) return [];

    if (typeof scale === 'string') {
        scale = convertMatchesArray(scale);
    }

    scale = scale || [];
    var len = scale.length;

    var colors = [];
    for (var i = 0; i < len - 1; i++) {
        for (var index = 0; index < count; index++) {
            colors.push(blend(scale[i], scale[i + 1], index / count));
        }
    }
    return colors;
}

function blend(startColor, endColor) {
    var ratio = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0.5;
    var format$$1 = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 'hex';

    var s = parse(startColor);
    var e = parse(endColor);

    return interpolateRGB(s, e, ratio, format$$1);
}

function mix(startcolor, endColor) {
    var ratio = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0.5;
    var format$$1 = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 'hex';

    return blend(startcolor, endColor, ratio, format$$1);
}

/**
 * 
 * @param {Color|String} c 
 */
function contrast(c$$1) {
    c$$1 = parse(c$$1);
    return (Math.round(c$$1.r * 299) + Math.round(c$$1.g * 587) + Math.round(c$$1.b * 114)) / 1000;
}

function contrastColor(c$$1) {
    return contrast(c$$1) >= 128 ? 'black' : 'white';
}

function gradient(colors) {
    var count = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 10;

    colors = parseGradient(colors);

    var newColors = [];
    var maxCount = count - (colors.length - 1);
    var allCount = maxCount;

    for (var i = 1, len = colors.length; i < len; i++) {

        var startColor = colors[i - 1][0];
        var endColor = colors[i][0];

        // if it is second color
        var rate = i == 1 ? colors[i][1] : colors[i][1] - colors[i - 1][1];

        // if it is last color 
        var colorCount = i == colors.length - 1 ? allCount : Math.floor(rate * maxCount);

        newColors = newColors.concat(scale([startColor, endColor], colorCount), [endColor]);

        allCount -= colorCount;
    }
    return newColors;
}

function scaleHSV(color) {
    var target = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'h';
    var count = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 9;
    var exportFormat = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 'rgb';
    var min = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 0;
    var max = arguments.length > 5 && arguments[5] !== undefined ? arguments[5] : 1;
    var dist = arguments.length > 6 && arguments[6] !== undefined ? arguments[6] : 100;

    var colorObj = parse(color);
    var hsv = RGBtoHSV(colorObj);
    var unit = (max - min) * dist / count;

    var results = [];
    for (var i = 1; i <= count; i++) {
        hsv[target] = Math.abs((dist - unit * i) / dist);
        results.push(format(HSVtoRGB(hsv), exportFormat));
    }

    return results;
}

function scaleH(color) {
    var count = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 9;
    var exportFormat = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 'rgb';
    var min = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 0;
    var max = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 360;

    return scaleHSV(color, 'h', count, exportFormat, min, max, 1);
}

function scaleS(color) {
    var count = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 9;
    var exportFormat = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 'rgb';
    var min = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 0;
    var max = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 1;

    return scaleHSV(color, 's', count, exportFormat, min, max, 100);
}

function scaleV(color) {
    var count = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 9;
    var exportFormat = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 'rgb';
    var min = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 0;
    var max = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 1;

    return scaleHSV(color, 'v', count, exportFormat, min, max, 100);
}

/* predefined scale colors */
scale.parula = function (count) {
    return scale(['#352a87', '#0f5cdd', '#00b5a6', '#ffc337', '#fdff00'], count);
};

scale.jet = function (count) {
    return scale(['#00008f', '#0020ff', '#00ffff', '#51ff77', '#fdff00', '#ff0000', '#800000'], count);
};

scale.hsv = function (count) {
    return scale(['#ff0000', '#ffff00', '#00ff00', '#00ffff', '#0000ff', '#ff00ff', '#ff0000'], count);
};

scale.hot = function (count) {
    return scale(['#0b0000', '#ff0000', '#ffff00', '#ffffff'], count);
};
scale.pink = function (count) {
    return scale(['#1e0000', '#bd7b7b', '#e7e5b2', '#ffffff'], count);
};

scale.bone = function (count) {
    return scale(['#000000', '#4a4a68', '#a6c6c6', '#ffffff'], count);
};

scale.copper = function (count) {
    return scale(['#000000', '#3d2618', '#9d623e', '#ffa167', '#ffc77f'], count);
};

var mixin = {
    interpolateRGB: interpolateRGB,
    blend: blend,
    mix: mix,
    scale: scale,
    contrast: contrast,
    contrastColor: contrastColor,
    gradient: gradient,
    scaleHSV: scaleHSV,
    scaleH: scaleH,
    scaleS: scaleS,
    scaleV: scaleV
};

function array_equals(v1, v2) {
    if (v1.length !== v2.length) return false;
    for (var i = 0, len = v1.length; i < len; ++i) {
        if (v1[i] !== v2[i]) return false;
    }
    return true;
}

function euclidean(v1, v2) {
    var total = 0;

    for (var i = 0, len = v1.length; i < len; i++) {
        total += Math.pow(v2[i] - v1[i], 2);
    }

    return Math.sqrt(total);
}

function manhattan(v1, v2) {
    var total = 0;

    for (var i = 0, len = v1.length; i < len; i++) {
        total += Math.abs(v2[i] - v1[i]);
    }

    return total;
}

function max(v1, v2) {
    var max = 0;
    for (var i = 0, len = v1.length; i < len; i++) {
        max = Math.max(max, Math.abs(v2[i] - v1[i]));
    }

    return max;
}

var distances = {
    euclidean: euclidean,
    manhattan: manhattan,
    max: max
};

var create_random_number = {
    linear: function linear(num, count) {
        var centeroids = [];
        var start = Math.round(Math.random() * num);
        var dist = Math.floor(num / count);

        do {

            centeroids.push(start);

            start = (start + dist) % num;
        } while (centeroids.length < count);

        return centeroids;
    },

    shuffle: function shuffle(num, count) {
        var centeroids = [];

        while (centeroids.length < count) {

            var index = Math.round(Math.random() * num);

            if (centeroids.indexOf(index) == -1) {
                centeroids.push(index);
            }
        }

        return centeroids;
    }

};

function randomCentroids(points, k) {
    var method = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 'linear';


    var centeroids = create_random_number[method](points.length, k);

    return centeroids.map(function (i) {
        return points[i];
    });

    // var centeroids = points.slice(0);

    // centeroids.sort(function () {
    //     return (Math.round(Math.random()) - 0.5);
    // })

    // return centeroids.slice(0, k); 
}

function closestCenteroid(point, centeroids, distance) {
    var min = Infinity,
        kIndex = 0;

    centeroids.forEach(function (center, i) {
        var dist = distance(point, center);

        if (dist < min) {
            min = dist;
            kIndex = i;
        }
    });

    return kIndex;
}

function getCenteroid(assigned) {

    if (!assigned.length) return [];

    // initialize centeroid list 
    var centeroid = new Array(assigned[0].length);
    for (var i = 0, len = centeroid.length; i < len; i++) {
        centeroid[i] = 0;
    }

    for (var index = 0, len = assigned.length; index < len; index++) {
        var it = assigned[index];

        var last = index + 1;

        for (var j = 0, jLen = it.length; j < jLen; j++) {
            centeroid[j] += (it[j] - centeroid[j]) / last;
        }
    }

    centeroid = centeroid.map(function (it) {
        return Math.floor(it);
    });

    return centeroid;
}

function unique_array(arrays) {
    return arrays;
    var set = {};
    var count = arrays.length;
    var it = null;
    while (count--) {
        it = arrays[count];
        set[JSON.stringify(it)] = it;
    }

    return Object.values(set);
}

function splitK(k, points, centeroids, distance) {
    var assignment = new Array(k);

    for (var i = 0; i < k; i++) {
        assignment[i] = [];
    }

    for (var idx = 0, pointLength = points.length; idx < pointLength; idx++) {
        var point = points[idx];
        var index = closestCenteroid(point, centeroids, distance);
        assignment[index].push(point);
    }

    return assignment;
}

function setNewCenteroid(k, points, assignment, centeroids, movement, randomFunction) {

    for (var i = 0; i < k; i++) {
        var assigned = assignment[i];

        var centeroid = centeroids[i];
        var newCenteroid = new Array(centeroid.length);

        if (assigned.length > 0) {
            newCenteroid = getCenteroid(assigned);
        } else {
            var idx = Math.floor(randomFunction() * points.length);
            newCenteroid = points[idx];
        }

        if (array_equals(newCenteroid, centeroid)) {
            movement = false;
        } else {
            movement = true;
        }

        centeroids[i] = newCenteroid;
    }

    return movement;
}

function kmeans(points, k, distanceFunction) {
    var period = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 10;
    var initialRandom = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 'linear';

    points = unique_array(points);

    k = k || Math.max(2, Math.ceil(Math.sqrt(points.length / 2)));

    var distance = distanceFunction || 'euclidean';
    if (typeof distance == 'string') {
        distance = distances[distance];
    }

    var rng_seed = 0;
    var random = function random() {
        rng_seed = (rng_seed * 9301 + 49297) % 233280;
        return rng_seed / 233280;
    };

    var centeroids = randomCentroids(points, k, initialRandom);

    var movement = true;
    var iterations = 0;
    while (movement) {
        var assignment = splitK(k, points, centeroids, distance);

        movement = setNewCenteroid(k, points, assignment, centeroids, false, random);

        iterations++;

        if (iterations % period == 0) {
            break;
        }
    }

    return centeroids;
}

function each(len, callback) {
    for (var i = 0; i < len; i += 4) {
        callback(i);
    }
}

function pack(bitmap, callback) {

    each(bitmap.pixels.length, function (i) {
        callback(bitmap.pixels, i);
    });
}

var Canvas = {
    create: function create(width, height) {
        var canvas = document.createElement('canvas');
        canvas.width = width || 0;
        canvas.height = height || 0;

        return canvas;
    },
    drawPixels: function drawPixels(bitmap) {
        var canvas = this.create(bitmap.width, bitmap.height);

        var context = canvas.getContext('2d');
        var imagedata = context.getImageData(0, 0, canvas.width, canvas.height);

        imagedata.data.set(bitmap.pixels);

        context.putImageData(imagedata, 0, 0);

        return canvas;
    },
    createHistogram: function createHistogram(width, height, histogram, callback) {
        var opt = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : { black: true, red: false, green: false, blue: false };

        var canvas = this.create(width, height);
        var context = canvas.getContext('2d');
        context.clearRect(0, 0, width, height);
        context.fillStyle = "white";
        context.fillRect(0, 0, width, height);
        context.globalAlpha = 0.7;

        var omit = { black: false };
        if (opt.black) {
            omit.black = false;
        } else {
            omit.black = true;
        }
        if (opt.red) {
            omit.red = false;
        } else {
            omit.red = true;
        }
        if (opt.green) {
            omit.green = false;
        } else {
            omit.green = true;
        }
        if (opt.blue) {
            omit.blue = false;
        } else {
            omit.blue = true;
        }

        Object.keys(histogram).forEach(function (color) {

            if (!omit[color]) {

                var array = histogram[color];
                var ymax = Math.max.apply(Math, array);
                var unitWith = width / array.length;

                context.fillStyle = color;
                array.forEach(function (it, index) {
                    var currentHeight = height * (it / ymax);
                    var x = index * unitWith;

                    context.fillRect(x, height - currentHeight, unitWith, currentHeight);
                });
            }
        });

        if (typeof callback == 'function') callback(canvas);
    },
    getHistogram: function getHistogram(bitmap) {
        var black = new Array(256);
        var red = new Array(256);
        var green = new Array(256);
        var blue = new Array(256);
        for (var i = 0; i < 256; i++) {
            black[i] = 0;
            red[i] = 0;
            green[i] = 0;
            blue[i] = 0;
        }

        pack(bitmap, function (pixels, i) {
            // gray scale 
            var grayIndex = Math.round(Color$1.brightness(pixels[i], pixels[i + 1], pixels[i + 2]));
            black[grayIndex]++;

            red[pixels[i]]++;
            green[pixels[i + 1]]++;
            blue[pixels[i + 2]]++;
        });

        return { black: black, red: red, green: green, blue: blue };
    },
    getBitmap: function getBitmap(bitmap, area) {
        var canvas = this.drawPixels(bitmap);

        var context = canvas.getContext('2d');
        var pixels = context.getImageData(area.x || 0, area.y || 0, area.width || canvas.width, area.height || canvas.height).data;

        return { pixels: pixels, width: area.width, height: area.height };
    },
    putBitmap: function putBitmap(bitmap, subBitmap, area) {

        var canvas = this.drawPixels(bitmap);
        var subCanvas = this.drawPixels(subBitmap);

        var context = canvas.getContext('2d');
        context.drawImage(subCanvas, area.x, area.y);

        bitmap.pixels = context.getImageData(0, 0, bitmap.width, bitmap.height).data;

        return bitmap;
    }
};

var ImageLoader = function () {
    function ImageLoader(url) {
        var opt = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
        classCallCheck(this, ImageLoader);

        this.isLoaded = false;
        this.imageUrl = url;
        this.opt = opt;
        this.initialize();
    }

    createClass(ImageLoader, [{
        key: 'initialize',
        value: function initialize() {
            this.canvas = this.createCanvas();
            this.context = this.canvas.getContext('2d');
        }
    }, {
        key: 'createCanvas',
        value: function createCanvas() {
            return document.createElement('canvas');
        }
    }, {
        key: 'load',
        value: function load(callback) {
            this.loadImage(callback);
        }
    }, {
        key: 'loadImage',
        value: function loadImage(callback) {
            var _this = this;

            var ctx = this.context;
            this.newImage = new Image();
            var img = this.newImage;
            img.onload = function () {
                var ratio = img.height / img.width;

                if (_this.opt.canvasWidth && _this.opt.canvasHeight) {
                    _this.canvas.width = _this.opt.canvasWidth;
                    _this.canvas.height = _this.opt.canvasHeight;
                } else {
                    _this.canvas.width = _this.opt.maxWidth ? _this.opt.maxWidth : img.width;
                    _this.canvas.height = _this.canvas.width * ratio;
                }

                ctx.drawImage(img, 0, 0, img.width, img.height, 0, 0, _this.canvas.width, _this.canvas.height);
                _this.isLoaded = true;
                callback && callback();
            };

            this.getImageUrl(function (url) {
                img.src = url;
            });
        }
    }, {
        key: 'load',
        value: function load(callback) {
            var _this2 = this;

            this.newImage = new Image();
            var img = this.newImage;
            img.onload = function () {
                _this2.isLoaded = true;
                callback && callback();
            };

            this.getImageUrl(function (url) {
                img.src = url;
            });
        }
    }, {
        key: 'getImageUrl',
        value: function getImageUrl(callback) {
            if (typeof this.imageUrl == 'string') {
                return callback(this.imageUrl);
            } else if (this.imageUrl instanceof Blob) {
                var reader = new FileReader();

                reader.onload = function (ev) {
                    callback(ev.target.result);
                };

                reader.readAsDataURL(this.imageUrl);
            }
        }
    }, {
        key: 'getRGBA',
        value: function getRGBA(r, g, b, a) {
            return [r, g, b, a];
        }
    }, {
        key: 'toArray',
        value: function toArray$$1(filter, callback) {
            var opt = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};

            var imagedata = this.context.getImageData(0, 0, this.canvas.width, this.canvas.height);
            var width = imagedata.width;
            var height = imagedata.height;

            var pixels = new Uint8ClampedArray(imagedata.data);

            var bitmap = { pixels: pixels, width: width, height: height };

            if (!filter) {
                filter = function () {
                    return function (bitmap, done) {
                        done(bitmap);
                    };
                }();
            }

            filter(bitmap, function (newBitmap) {
                var tmpCanvas = Canvas.drawPixels(newBitmap);

                if (opt.returnTo == 'canvas') {
                    callback(tmpCanvas);
                } else {
                    callback(tmpCanvas.toDataURL(opt.outputFormat || 'image/png'));
                }
            }, opt);
        }
    }, {
        key: 'toHistogram',
        value: function toHistogram(opt) {
            var imagedata = this.context.getImageData(0, 0, this.canvas.width, this.canvas.height);
            var width = imagedata.width;
            var height = imagedata.height;

            var pixels = new Uint8ClampedArray(imagedata.data);

            var bitmap = { pixels: pixels, width: width, height: height };

            return Canvas.getHistogram(bitmap);
        }
    }, {
        key: 'toRGB',
        value: function toRGB() {
            var imagedata = this.context.getImageData(0, 0, this.canvas.width, this.canvas.height);

            var rgba = imagedata.data;
            var results = [];
            for (var i = 0, len = rgba.length; i < len; i += 4) {
                results[results.length] = [rgba[i + 0], rgba[i + 1], rgba[i + 2], rgba[i + 3]];
            }

            return results;
        }
    }]);
    return ImageLoader;
}();

var CONSTANT = {
    identity: function identity() {
        return [1, 0, 0, 0, 1, 0, 0, 0, 1];
    },
    stretching: function stretching(k) {
        return [k, 0, 0, 0, 1, 0, 0, 0, 1];
    },
    squeezing: function squeezing(k) {
        return [k, 0, 0, 0, 1 / k, 0, 0, 0, 1];
    },
    scale: function scale() {
        var sx = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;
        var sy = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 1;

        sx = sx || sx === 0 ? sx : 1;
        sy = sy || sy === 0 ? sy : 1;
        return [sx, 0, 0, 0, sy, 0, 0, 0, 1];
    },
    scaleX: function scaleX(sx) {
        return this.scale(sx);
    },
    scaleY: function scaleY(sy) {
        return this.scale(1, sy);
    },
    translate: function translate(tx, ty) {
        return [1, 0, tx, 0, 1, ty, 0, 0, 1];
    },
    rotate: function rotate(angle) {
        var r = this.radian(angle);
        return [Math.cos(r), -Math.sin(r), 0, Math.sin(r), Math.cos(r), 0, 0, 0, 1];
    },
    rotate90: function rotate90() {
        return [0, -1, 0, 1, 0, 0, 0, 0, 1];
    },
    rotate180: function rotate180() {
        return [-1, 0, 0, 0, -1, 0, 0, 0, 1];
    },
    rotate270: function rotate270() {
        return [0, 1, 0, -1, 0, 0, 0, 0, 1];
    },
    radian: function radian(degree) {
        return degree * Math.PI / 180;
    },
    skew: function skew(degreeX, degreeY) {
        var radianX = this.radian(degreeX);
        var radianY = this.radian(degreeY);
        return [1, Math.tan(radianX), 0, Math.tan(radianY), 1, 0, 0, 0, 1];
    },
    skewX: function skewX(degreeX) {
        var radianX = this.radian(degreeX);

        return [1, Math.tan(radianX), 0, 0, 1, 0, 0, 0, 1];
    },
    skewY: function skewY(degreeY) {
        var radianY = this.radian(degreeY);

        return [1, 0, 0, Math.tan(radianY), 1, 0, 0, 0, 1];
    },
    shear1: function shear1(angle) {
        return [1, -Math.tan(this.radian(angle) / 2), 0, 0, 1, 0, 0, 0, 1];
    },
    shear2: function shear2(angle) {
        return [1, 0, 0, Math.sin(this.radian(angle)), 1, 0, 0, 0, 1];
    }
};

var Matrix = {
    CONSTANT: CONSTANT,

    radian: function radian(angle) {
        return CONSTANT.radian(angle);
    },
    multiply: function multiply(A, C) {
        // console.log(JSON.stringify(A), JSON.stringify(C))
        return [A[0] * C[0] + A[1] * C[1] + A[2] * C[2], A[3] * C[0] + A[4] * C[1] + A[5] * C[2], A[6] * C[0] + A[7] * C[1] + A[8] * C[2]];
    },
    identity: function identity(B) {
        return this.multiply(CONSTANT.identity(), B);
    },
    translate: function translate(x, y, B) {
        return this.multiply(CONSTANT.translate(x, y), B);
    },
    rotate: function rotate(angle, B) {
        return this.multiply(CONSTANT.rotate(angle), B);
    },
    shear1: function shear1(angle, B) {
        return this.multiply(CONSTANT.shear1(angle), B);
    },
    shear2: function shear2(angle, B) {
        return this.multiply(CONSTANT.shear2(angle), B);
    },
    rotateShear: function rotateShear(angle, B) {

        var arr = B;

        arr = this.shear1(angle, arr);
        arr = this.shear2(angle, arr);
        arr = this.shear1(angle, arr);

        return arr;
    }
};

function crop() {
    var startX = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;
    var startY = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
    var width = arguments[2];
    var height = arguments[3];


    var newBitmap = createBitmap(width * height * 4, width, height);

    return function (bitmap, done) {
        for (var y = startY, realY = 0; y < height; y++, realY++) {
            for (var x = startX, realX = 0; x < width; x++, realX++) {
                newBitmap.pixels[realY * width * realX] = bitmap.pixels[y * width * x];
            }
        }

        done(newBitmap);
    };
}

// Image manupulate 
function resize(dstWidth, dstHeight) {
    return function (bitmap, done) {
        var c = Canvas.drawPixels(bitmap);
        var context = c.getContext('2d');

        c.width = dstWidth;
        c.height = dstHeight;

        done({
            pixels: new Uint8ClampedArray(context.getImageData(0, 0, dstWidth, dstHeight).data),
            width: dstWidth,
            height: dstHeight
        });
    };
}

function flipV() {
    return function (bitmap, done) {
        var width = bitmap.width;
        var height = bitmap.height;
        var isCenter = height % 2 == 1 ? 1 : 0;

        var halfHeight = isCenter ? Math.floor(height / 2) : height / 2;

        for (var y = 0; y < halfHeight; y++) {
            for (var x = 0; x < width; x++) {

                var startIndex = y * width + x << 2;
                var endIndex = (height - 1 - y) * width + x << 2;
                swapColor(bitmap.pixels, startIndex, endIndex);
            }
        }

        done(bitmap);
    };
}

function flipH() {
    return function (bitmap, done) {
        var width = bitmap.width;
        var height = bitmap.height;
        var isCenter = width % 2 == 1 ? 1 : 0;

        var halfWidth = isCenter ? Math.floor(width / 2) : width / 2;

        for (var y = 0; y < height; y++) {
            for (var x = 0; x < halfWidth; x++) {

                var startIndex = y * width + x << 2;
                var endIndex = y * width + (width - 1 - x) << 2;
                swapColor(bitmap.pixels, startIndex, endIndex);
            }
        }

        done(bitmap);
    };
}

function rotateDegree(angle) {
    var cx = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'center';
    var cy = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 'center';

    // const r = F.radian(angle)

    return function (bitmap, done) {
        var opt = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};

        var newBitmap = createBitmap(bitmap.pixels.length, bitmap.width, bitmap.height);
        var width = bitmap.width;
        var height = bitmap.height;

        if (cx == 'center') {
            cx = Math.floor(width / 2);
        }

        if (cy == 'center') {
            cy = Math.floor(height / 2);
        }

        var translateMatrix = Matrix.CONSTANT.translate(-cx, -cy);
        var translateMatrix2 = Matrix.CONSTANT.translate(cx, cy);
        var shear1Matrix = Matrix.CONSTANT.shear1(angle);
        var shear2Matrix = Matrix.CONSTANT.shear2(angle);

        packXY(function (pixels, i, x, y) {
            // console.log(x, y, i)
            var arr = Matrix.multiply(translateMatrix, [x, y, 1]);

            arr = Matrix.multiply(shear1Matrix, arr).map(Math.round);
            arr = Matrix.multiply(shear2Matrix, arr).map(Math.round);
            arr = Matrix.multiply(shear1Matrix, arr).map(Math.round);
            arr = Matrix.multiply(translateMatrix2, arr);

            var _arr = arr,
                _arr2 = slicedToArray(_arr, 2),
                x1 = _arr2[0],
                y1 = _arr2[1];

            if (x1 < 0) return;
            if (y1 < 0) return;
            if (x1 > width - 1) return;
            if (y1 > height - 1) return;

            var endIndex = y1 * width + x1 << 2; //  bit 2 shift is  * 4  

            fillPixelColor(pixels, endIndex, bitmap.pixels, i);
        })(newBitmap, function () {
            done(newBitmap);
        }, opt);
    };
}

function rotate() {
    var degree = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;

    degree = parseParamNumber(degree);
    degree = degree % 360;
    return function (bitmap, done) {
        var opt = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};


        if (degree == 0) return bitmap;

        if (degree == 90 || degree == 270) {
            var newBitmap = createBitmap(bitmap.pixels.length, bitmap.height, bitmap.width);
        } else if (degree == 180) {
            var newBitmap = createBitmap(bitmap.pixels.length, bitmap.width, bitmap.height);
        } else {
            return rotateDegree(degree)(bitmap, done, opt);
        }
        packXY(function (pixels, i, x, y) {

            if (degree == 90) {
                var endIndex = x * newBitmap.width + (newBitmap.width - 1 - y) << 2; //  << 2 is equals to (multiply)* 4 
            } else if (degree == 270) {
                var endIndex = (newBitmap.height - 1 - x) * newBitmap.width + y << 2;
            } else if (degree == 180) {
                var endIndex = (newBitmap.height - 1 - y) * newBitmap.width + (newBitmap.width - 1 - x) << 2;
            }

            fillPixelColor(newBitmap.pixels, endIndex, bitmap.pixels, i);
        })(bitmap, function () {
            done(newBitmap);
        }, opt);
    };
}

function histogram$1() {
    var type = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 'gray';
    var points = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : [];

    var $realPoints = [];

    for (var i = 0; i < points.length - 1; i++) {
        var sp = points[i];
        var ep = points[i + 1];

        var distX = ep[0] - sp[0];
        var distY = ep[1] - sp[1];

        var rate = distY / distX;

        for (var realIndex = 0, start = sp[0]; realIndex < distX; realIndex++, start++) {
            $realPoints[start] = sp[1] + realIndex * rate;
        }
    }

    $realPoints[255] = 255;

    if (type === 'red') {
        return pixel(function () {
            $r = $realPoints[$r];
        }, {}, { $realPoints: $realPoints });
    } else if (type === 'green') {
        return pixel(function () {
            $g = $realPoints[$g];
        }, {}, { $realPoints: $realPoints });
    } else if (type === 'blue') {
        return pixel(function () {
            $b = $realPoints[$b];
        }, {}, { $realPoints: $realPoints });
    } else {
        return pixel(function () {

            var l = Color.RGBtoYCrCb($r, $g, $b);
            var c = Color.YCrCbtoRGB(clamp($realPoints[clamp(l.y)]), l.cr, l.cb, 0);
            $r = c.r;
            $g = c.g;
            $b = c.b;
        }, {}, { $realPoints: $realPoints });
    }
}

var image$1 = {
    crop: crop,
    resize: resize,
    flipH: flipH,
    flipV: flipV,
    rotate: rotate,
    rotateDegree: rotateDegree,
    histogram: histogram$1,
    'rotate-degree': rotateDegree
};

function bitonal(darkColor, lightColor) {
    var threshold = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 100;

    var $darkColor = Color$1.parse(darkColor);
    var $lightColor = Color$1.parse(lightColor);
    var $threshold = threshold;

    return pixel('\n        const thresholdColor = ( $r + $g + $b ) <= $threshold ? $darkColor : $lightColor\n\n        $r = thresholdColor.r\n        $g = thresholdColor.g \n        $b = thresholdColor.b \n    ', {
        $threshold: $threshold
    }, {
        $darkColor: $darkColor,
        $lightColor: $lightColor
    });
}

/*
 * @param {Number} amount  -100..100  ,  value < 0  is darken, value > 0 is brighten 
 */
function brightness$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    amount = parseParamNumber(amount);
    var $C = Math.floor(255 * (amount / 100));

    return pixel('\n        $r += $C \n        $g += $C \n        $b += $C \n    ', { $C: $C });
}

function brownie() {

    var $matrix = [0.5997023498159715, 0.34553243048391263, -0.2708298674538042, 0, -0.037703249837783157, 0.8609577587992641, 0.15059552388459913, 0, 0.24113635128153335, -0.07441037908422492, 0.44972182064877153, 0, 0, 0, 0, 1];

    return pixel('\n        $r = $matrix[0] * $r + $matrix[1] * $g + $matrix[2] * $b + $matrix[3] * $a\n        $g = $matrix[4] * $r + $matrix[5] * $g + $matrix[6] * $b + $matrix[7] * $a\n        $b = $matrix[8] * $r + $matrix[9] * $g + $matrix[10] * $b + $matrix[11] * $a\n        $a = $matrix[12] * $r + $matrix[13] * $g + $matrix[14] * $b + $matrix[15] * $a        \n    ', {
        $matrix: $matrix
    });
}

/**
 * 
 * @param {Number} amount from 0 to 100 
 */
function clip() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;

    amount = parseParamNumber(amount);
    var $C = Math.abs(amount) * 2.55;

    return pixel('\n\n        $r = ($r > 255 - $C) ? 255 : 0\n        $g = ($g > 255 - $C) ? 255 : 0\n        $b = ($b > 255 - $C) ? 255 : 0\n\n    ', { $C: $C });
}

/**
 * 
 * @param {*} amount   min = -128, max = 128 
 */
function contrast$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;

    amount = parseParamNumber(amount);
    var $C = Math.max((128 + amount) / 128, 0);

    return pixel('\n        $r *= $C\n        $g *= $C\n        $b *= $C\n    ', { $C: $C });
}

function gamma() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var $C = parseParamNumber(amount);
    return pixel('\n        $r = Math.pow($r / 255, $C) * 255\n        $g = Math.pow($g / 255, $C) * 255\n        $b = Math.pow($b / 255, $C) * 255\n    ', { $C: $C });
}

/**
 * F.gradient('red', 'blue', 'yellow', 'white', 10)
 * F.gradient('red, blue, yellow, white, 10')
 */
function gradient$1() {
    // 전체 매개변수 기준으로 파싱 
    // 색이 아닌 것 기준으로 scale 변수로 인식 

    var params = [].concat(Array.prototype.slice.call(arguments));

    if (params.length === 1 && typeof params[0] === 'string') {
        params = Color$1.convertMatchesArray(params[0]);
    }

    params = params.map(function (arg) {
        var res = Color$1.matches(arg);

        if (!res.length) {
            return { type: 'scale', value: arg };
        }

        return { type: 'param', value: arg };
    });

    var $scale = params.filter(function (it) {
        return it.type == 'scale';
    })[0];
    $scale = $scale ? +$scale.value : 256;

    params = params.filter(function (it) {
        return it.type == 'param';
    }).map(function (it) {
        return it.value;
    }).join(',');

    var $colors = Color$1.gradient(params, $scale).map(function (c) {
        var _Color$parse = Color$1.parse(c),
            r = _Color$parse.r,
            g = _Color$parse.g,
            b = _Color$parse.b,
            a = _Color$parse.a;

        return { r: r, g: g, b: b, a: a };
    });

    return pixel('\n        const colorIndex = clamp(Math.ceil($r * 0.2126 + $g * 0.7152 + $b * 0.0722))\n        const newColorIndex = clamp(Math.floor(colorIndex * ($scale / 256)))\n        const color = $colors[newColorIndex]\n\n        $r = color.r \n        $g = color.g \n        $b = color.b \n        $a = clamp(Math.floor(color.a * 256))\n    ', {}, { $colors: $colors, $scale: $scale });
}

function grayscale(amount) {
    amount = parseParamNumber(amount);
    var C = amount / 100;

    if (C > 1) C = 1;

    var $matrix = [0.2126 + 0.7874 * (1 - C), 0.7152 - 0.7152 * (1 - C), 0.0722 - 0.0722 * (1 - C), 0, 0.2126 - 0.2126 * (1 - C), 0.7152 + 0.2848 * (1 - C), 0.0722 - 0.0722 * (1 - C), 0, 0.2126 - 0.2126 * (1 - C), 0.7152 - 0.7152 * (1 - C), 0.0722 + 0.9278 * (1 - C), 0, 0, 0, 0, 1];

    return pixel( /*javascript*/'\n        $r = $matrix[0] * $r + $matrix[1] * $g + $matrix[2] * $b + $matrix[3] * $a\n        $g = $matrix[4] * $r + $matrix[5] * $g + $matrix[6] * $b + $matrix[7] * $a\n        $b = $matrix[8] * $r + $matrix[9] * $g + $matrix[10] * $b + $matrix[11] * $a\n        $a = $matrix[12] * $r + $matrix[13] * $g + $matrix[14] * $b + $matrix[15] * $a\n    ', {
        $matrix: $matrix
    });
}

/*
 * @param {Number} amount   0..360  
 */
function hue() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 360;

    var $C = parseParamNumber(amount);
    return pixel('\n        var hsv = Color.RGBtoHSV($r, $g, $b);\n\n        // 0 ~ 360 \n        var h = hsv.h;\n        h += Math.abs($C)\n        h = h % 360\n        hsv.h = h\n\n        var rgb = Color.HSVtoRGB(hsv);\n\n        $r = rgb.r\n        $g = rgb.g\n        $b = rgb.b\n    ', {
        $C: $C
    });
}

function invert() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    var $C = amount / 100;

    return pixel('\n        $r = (255 - $r) * $C\n        $g = (255 - $g) * $C\n        $b = (255 - $b) * $C\n    ', {
        $C: $C
    });
}

function kodachrome() {

    var $matrix = [1.1285582396593525, -0.3967382283601348, -0.03992559172921793, 0, -0.16404339962244616, 1.0835251566291304, -0.05498805115633132, 0, -0.16786010706155763, -0.5603416277695248, 1.6014850761964943, 0, 0, 0, 0, 1];

    return pixel('\n        $r = $matrix[0] * $r + $matrix[1] * $g + $matrix[2] * $b + $matrix[3] * $a\n        $g = $matrix[4] * $r + $matrix[5] * $g + $matrix[6] * $b + $matrix[7] * $a\n        $b = $matrix[8] * $r + $matrix[9] * $g + $matrix[10] * $b + $matrix[11] * $a\n        $a = $matrix[12] * $r + $matrix[13] * $g + $matrix[14] * $b + $matrix[15] * $a        \n    ', {
        $matrix: $matrix
    });
}

function matrix() {
    var $a = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;
    var $b = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
    var $c = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0;
    var $d = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 0;
    var $e = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 0;
    var $f = arguments.length > 5 && arguments[5] !== undefined ? arguments[5] : 0;
    var $g = arguments.length > 6 && arguments[6] !== undefined ? arguments[6] : 0;
    var $h = arguments.length > 7 && arguments[7] !== undefined ? arguments[7] : 0;
    var $i = arguments.length > 8 && arguments[8] !== undefined ? arguments[8] : 0;
    var $j = arguments.length > 9 && arguments[9] !== undefined ? arguments[9] : 0;
    var $k = arguments.length > 10 && arguments[10] !== undefined ? arguments[10] : 0;
    var $l = arguments.length > 11 && arguments[11] !== undefined ? arguments[11] : 0;
    var $m = arguments.length > 12 && arguments[12] !== undefined ? arguments[12] : 0;
    var $n = arguments.length > 13 && arguments[13] !== undefined ? arguments[13] : 0;
    var $o = arguments.length > 14 && arguments[14] !== undefined ? arguments[14] : 0;
    var $p = arguments.length > 15 && arguments[15] !== undefined ? arguments[15] : 0;


    var $matrix = [$a, $b, $c, $d, $e, $f, $g, $h, $i, $j, $k, $l, $m, $n, $o, $p];

    return pixel('\n        $r = $matrix[0] * $r + $matrix[1] * $g + $matrix[2] * $b + $matrix[3] * $a\n        $g = $matrix[4] * $r + $matrix[5] * $g + $matrix[6] * $b + $matrix[7] * $a\n        $b = $matrix[8] * $r + $matrix[9] * $g + $matrix[10] * $b + $matrix[11] * $a\n        $a = $matrix[12] * $r + $matrix[13] * $g + $matrix[14] * $b + $matrix[15] * $a        \n    ', {
        $matrix: $matrix
    });
}

/**
 * 
 * @param {Number} amount 1..100
 */
function noise() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var $C = parseParamNumber(amount);
    return pixel('\n        const C = Math.abs($C) * 5\n        const min = -C\n        const max = C \n        const noiseValue = Math.round(min + (Math.random() * (max - min)))\n\n        $r += noiseValue\n        $g += noiseValue\n        $b += noiseValue\n    ', {
        $C: $C
    });
}

function opacity() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    var $C = amount / 100;

    return pixel('\n        $a *= $C \n    ', { $C: $C });
}

function polaroid() {

    var $matrix = [1.438, -0.062, -0.062, 0, -0.122, 1.378, -0.122, 0, -0.016, -0.016, 1.483, 0, 0, 0, 0, 1];

    return pixel('\n        $r = $matrix[0] * $r + $matrix[1] * $g + $matrix[2] * $b + $matrix[3] * $a\n        $g = $matrix[4] * $r + $matrix[5] * $g + $matrix[6] * $b + $matrix[7] * $a\n        $b = $matrix[8] * $r + $matrix[9] * $g + $matrix[10] * $b + $matrix[11] * $a\n        $a = $matrix[12] * $r + $matrix[13] * $g + $matrix[14] * $b + $matrix[15] * $a        \n    ', {
        $matrix: $matrix
    });
}

/*
 * @param {Number} amount  -100..100 
 */
function saturation() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    var C = amount / 100;
    var L = 1 - Math.abs(C);

    var $matrix = [L, 0, 0, 0, 0, L, 0, 0, 0, 0, L, 0, 0, 0, 0, L];

    return pixel('\n        $r = $matrix[0] * $r + $matrix[1] * $g + $matrix[2] * $b + $matrix[3] * $a\n        $g = $matrix[4] * $r + $matrix[5] * $g + $matrix[6] * $b + $matrix[7] * $a\n        $b = $matrix[8] * $r + $matrix[9] * $g + $matrix[10] * $b + $matrix[11] * $a\n        $a = $matrix[12] * $r + $matrix[13] * $g + $matrix[14] * $b + $matrix[15] * $a        \n    ', {
        $matrix: $matrix
    });
}

/*
 * @param {Number} amount  0..1 
 */
function sepia() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var C = parseParamNumber(amount);
    if (C > 1) C = 1;

    var $matrix = [0.393 + 0.607 * (1 - C), 0.769 - 0.769 * (1 - C), 0.189 - 0.189 * (1 - C), 0, 0.349 - 0.349 * (1 - C), 0.686 + 0.314 * (1 - C), 0.168 - 0.168 * (1 - C), 0, 0.272 - 0.272 * (1 - C), 0.534 - 0.534 * (1 - C), 0.131 + 0.869 * (1 - C), 0, 0, 0, 0, 1];

    return pixel('\n        $r = $matrix[0] * $r + $matrix[1] * $g + $matrix[2] * $b + $matrix[3] * $a\n        $g = $matrix[4] * $r + $matrix[5] * $g + $matrix[6] * $b + $matrix[7] * $a\n        $b = $matrix[8] * $r + $matrix[9] * $g + $matrix[10] * $b + $matrix[11] * $a\n        $a = $matrix[12] * $r + $matrix[13] * $g + $matrix[14] * $b + $matrix[15] * $a        \n    ', {
        $matrix: $matrix
    });
}

function shade() {
    var redValue = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;
    var greenValue = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 1;
    var blueValue = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 1;

    var $redValue = parseParamNumber(redValue);
    var $greenValue = parseParamNumber(greenValue);
    var $blueValue = parseParamNumber(blueValue);

    return pixel('\n        $r *= $redValue\n        $g *= $greenValue\n        $b *= $blueValue\n    ', {
        $redValue: $redValue,
        $greenValue: $greenValue,
        $blueValue: $blueValue
    });
}

function shift() {

    var $matrix = [1.438, -0.062, -0.062, 0, -0.122, 1.378, -0.122, 0, -0.016, -0.016, 1.483, 0, 0, 0, 0, 1];

    return pixel('\n        $r = $matrix[0] * $r + $matrix[1] * $g + $matrix[2] * $b + $matrix[3] * $a\n        $g = $matrix[4] * $r + $matrix[5] * $g + $matrix[6] * $b + $matrix[7] * $a\n        $b = $matrix[8] * $r + $matrix[9] * $g + $matrix[10] * $b + $matrix[11] * $a\n        $a = $matrix[12] * $r + $matrix[13] * $g + $matrix[14] * $b + $matrix[15] * $a        \n    ', {
        $matrix: $matrix
    });
}

/**
 * change the relative darkness of (a part of an image) by overexposure to light.
 * @param {*} r 
 * @param {*} g 
 * @param {*} b 
 */
function solarize(redValue, greenValue, blueValue) {
    var $redValue = parseParamNumber(redValue);
    var $greenValue = parseParamNumber(greenValue);
    var $blueValue = parseParamNumber(blueValue);
    return pixel('\n        $r = ($r < $redValue) ? 255 - $r: $r\n        $g = ($g < $greenValue) ? 255 - $g: $g\n        $b = ($b < $blueValue) ? 255 - $b: $b\n    ', {
        $redValue: $redValue, $greenValue: $greenValue, $blueValue: $blueValue
    });
}

function technicolor() {

    var $matrix = [1.9125277891456083, -0.8545344976951645, -0.09155508482755585, 0, -0.3087833385928097, 1.7658908555458428, -0.10601743074722245, 0, -0.231103377548616, -0.7501899197440212, 1.847597816108189, 0, 0, 0, 0, 1];

    return pixel('\n        $r = $matrix[0] * $r + $matrix[1] * $g + $matrix[2] * $b + $matrix[3] * $a\n        $g = $matrix[4] * $r + $matrix[5] * $g + $matrix[6] * $b + $matrix[7] * $a\n        $b = $matrix[8] * $r + $matrix[9] * $g + $matrix[10] * $b + $matrix[11] * $a\n        $a = $matrix[12] * $r + $matrix[13] * $g + $matrix[14] * $b + $matrix[15] * $a        \n    ', {
        $matrix: $matrix
    });
}

function thresholdColor() {
    var scale = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 200;
    var amount = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 100;
    var hasColor = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : true;

    var $scale = parseParamNumber(scale);
    amount = parseParamNumber(amount);
    var $C = amount / 100;
    var $hasColor = hasColor;

    return pixel('\n        // refer to Color.brightness \n        const v = ($C * Math.ceil($r * 0.2126 + $g * 0.7152 + $b * 0.0722) ) >= $scale ? 255 : 0;\n\n        if ($hasColor) {\n\n            if (v == 0) {\n                $r = 0 \n                $g = 0 \n                $b = 0\n            }\n            \n        } else {\n            const value = Math.round(v)\n            $r = value \n            $g = value \n            $b = value \n        }\n        \n    ', {
        $C: $C, $scale: $scale, $hasColor: $hasColor
    });
}

/*
 * @param {Number} amount  0..100 
 */
function threshold() {
  var scale = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 200;
  var amount = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 100;

  return thresholdColor(scale, amount, false);
}

function tint () {
    var redTint = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;
    var greenTint = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 1;
    var blueTint = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 1;

    var $redTint = parseParamNumber(redTint);
    var $greenTint = parseParamNumber(greenTint);
    var $blueTint = parseParamNumber(blueTint);
    return pixel("\n\n        $r += (255 - $r) * $redTint\n        $g += (255 - $g) * $greenTint\n        $b += (255 - $b) * $blueTint\n\n    ", {
        $redTint: $redTint,
        $greenTint: $greenTint,
        $blueTint: $blueTint
    });
}

var pixel$1 = {
    bitonal: bitonal,
    brightness: brightness$1,
    brownie: brownie,
    clip: clip,
    contrast: contrast$1,
    gamma: gamma,
    gradient: gradient$1,
    grayscale: grayscale,
    hue: hue,
    invert: invert,
    kodachrome: kodachrome,
    matrix: matrix,
    noise: noise,
    opacity: opacity,
    polaroid: polaroid,
    saturation: saturation,
    sepia: sepia,
    shade: shade,
    shift: shift,
    solarize: solarize,
    technicolor: technicolor,
    threshold: threshold,
    'threshold-color': thresholdColor,
    tint: tint
};

function blur () {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 3;
    amount = parseParamNumber(amount);

    return convolution(createBlurMatrix(amount));
}

/*
 * carve, mold, or stamp a design on (a surface) so that it stands out in relief.
 * 
 * @param {Number} amount   0.0 .. 4.0 
 */
function emboss() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 4;

    amount = parseParamNumber(amount);
    return convolution([amount * -2.0, -amount, 0.0, -amount, 1.0, amount, 0.0, amount, amount * 2.0]);
}

function gaussianBlur() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    var C = amount / 100;

    return convolution(weight([1, 2, 1, 2, 4, 2, 1, 2, 1], 1 / 16 * C));
}

function gaussianBlur5x() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    var C = amount / 100;
    return convolution(weight([1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4, 1], 1 / 256 * C));
}

function grayscale2() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    return convolution(weight([0.3, 0.3, 0.3, 0, 0, 0.59, 0.59, 0.59, 0, 0, 0.11, 0.11, 0.11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], amount / 100));
}

function identity() {
    return convolution([0, 0, 0, 0, 1, 0, 0, 0, 0]);
}

function kirschHorizontal() {
    var count = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    count = parseParamNumber(count);
    return convolution([5, 5, 5, -3, 0, -3, -3, -3, -3]);
}

function kirschVertical() {
    var count = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    count = parseParamNumber(count);
    return convolution([5, -3, -3, 5, 0, -3, 5, -3, -3]);
}

function laplacian() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    return convolution(weight([-1, -1, -1, -1, 8, -1, -1, -1, -1], amount / 100));
}

function laplacian5x() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    return convolution(weight([-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1], amount / 100));
}

function motionBlur() {
    return convolution(weight([1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1], 1 / 9));
}

function motionBlur2() {
    return convolution(weight([1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1], 1 / 9));
}

function motionBlur3() {
    return convolution(weight([1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1], 1 / 9));
}

function negative() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    return convolution(weight([-1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1], amount / 100));
}

function sepia2() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    return convolution(weight([0.393, 0.349, 0.272, 0, 0, 0.769, 0.686, 0.534, 0, 0, 0.189, 0.168, 0.131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0], amount / 100));
}

function sharpen() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    return convolution(weight([0, -1, 0, -1, 5, -1, 0, -1, 0], amount / 100));
}

function sobelHorizontal() {
    return convolution([-1, -2, -1, 0, 0, 0, 1, 2, 1]);
}

function sobelVertical() {
    return convolution([-1, 0, 1, -2, 0, 2, -1, 0, 1]);
}

/*

StackBlur - a fast almost Gaussian Blur For Canvas

Version: 	0.5
Author:		Mario Klingemann
Contact: 	mario@quasimondo.com
Website:	http://www.quasimondo.com/StackBlurForCanvas
Twitter:	@quasimondo

In case you find this class useful - especially in commercial projects -
I am not totally unhappy for a small donation to my PayPal account
mario@quasimondo.de

Or support me on flattr: 
https://flattr.com/thing/72791/StackBlur-a-fast-almost-Gaussian-Blur-Effect-for-CanvasJavascript

Copyright (c) 2010 Mario Klingemann

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

var mul_table = [512, 512, 456, 512, 328, 456, 335, 512, 405, 328, 271, 456, 388, 335, 292, 512, 454, 405, 364, 328, 298, 271, 496, 456, 420, 388, 360, 335, 312, 292, 273, 512, 482, 454, 428, 405, 383, 364, 345, 328, 312, 298, 284, 271, 259, 496, 475, 456, 437, 420, 404, 388, 374, 360, 347, 335, 323, 312, 302, 292, 282, 273, 265, 512, 497, 482, 468, 454, 441, 428, 417, 405, 394, 383, 373, 364, 354, 345, 337, 328, 320, 312, 305, 298, 291, 284, 278, 271, 265, 259, 507, 496, 485, 475, 465, 456, 446, 437, 428, 420, 412, 404, 396, 388, 381, 374, 367, 360, 354, 347, 341, 335, 329, 323, 318, 312, 307, 302, 297, 292, 287, 282, 278, 273, 269, 265, 261, 512, 505, 497, 489, 482, 475, 468, 461, 454, 447, 441, 435, 428, 422, 417, 411, 405, 399, 394, 389, 383, 378, 373, 368, 364, 359, 354, 350, 345, 341, 337, 332, 328, 324, 320, 316, 312, 309, 305, 301, 298, 294, 291, 287, 284, 281, 278, 274, 271, 268, 265, 262, 259, 257, 507, 501, 496, 491, 485, 480, 475, 470, 465, 460, 456, 451, 446, 442, 437, 433, 428, 424, 420, 416, 412, 408, 404, 400, 396, 392, 388, 385, 381, 377, 374, 370, 367, 363, 360, 357, 354, 350, 347, 344, 341, 338, 335, 332, 329, 326, 323, 320, 318, 315, 312, 310, 307, 304, 302, 299, 297, 294, 292, 289, 287, 285, 282, 280, 278, 275, 273, 271, 269, 267, 265, 263, 261, 259];

var shg_table = [9, 11, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24];

function BlurStack() {
    this.r = 0;
    this.g = 0;
    this.b = 0;
    this.a = 0;
    this.next = null;
}

function stackBlurImage(bitmap, radius, blurAlphaChannel) {

    if (blurAlphaChannel) return stackBlurCanvasRGBA(bitmap, 0, 0, radius);else return stackBlurCanvasRGB(bitmap, 0, 0, radius);
}

function stackBlurCanvasRGBA(bitmap, top_x, top_y, radius) {
    if (isNaN(radius) || radius < 1) return bitmap;
    radius |= 0;

    var pixels = bitmap.pixels,
        width = bitmap.width,
        height = bitmap.height;

    var x, y, i, p, yp, yi, yw, r_sum, g_sum, b_sum, a_sum, r_out_sum, g_out_sum, b_out_sum, a_out_sum, r_in_sum, g_in_sum, b_in_sum, a_in_sum, pr, pg, pb, pa, rbs;

    var div = radius + radius + 1;
    var widthMinus1 = width - 1;
    var heightMinus1 = height - 1;
    var radiusPlus1 = radius + 1;
    var sumFactor = radiusPlus1 * (radiusPlus1 + 1) / 2;

    var stackStart = new BlurStack();
    var stack = stackStart;
    for (i = 1; i < div; i++) {
        stack = stack.next = new BlurStack();
        if (i == radiusPlus1) var stackEnd = stack;
    }
    stack.next = stackStart;
    var stackIn = null;
    var stackOut = null;

    yw = yi = 0;

    var mul_sum = mul_table[radius];
    var shg_sum = shg_table[radius];

    for (y = 0; y < height; y++) {
        r_in_sum = g_in_sum = b_in_sum = a_in_sum = r_sum = g_sum = b_sum = a_sum = 0;

        r_out_sum = radiusPlus1 * (pr = pixels[yi]);
        g_out_sum = radiusPlus1 * (pg = pixels[yi + 1]);
        b_out_sum = radiusPlus1 * (pb = pixels[yi + 2]);
        a_out_sum = radiusPlus1 * (pa = pixels[yi + 3]);

        r_sum += sumFactor * pr;
        g_sum += sumFactor * pg;
        b_sum += sumFactor * pb;
        a_sum += sumFactor * pa;

        stack = stackStart;

        for (i = 0; i < radiusPlus1; i++) {
            stack.r = pr;
            stack.g = pg;
            stack.b = pb;
            stack.a = pa;
            stack = stack.next;
        }

        for (i = 1; i < radiusPlus1; i++) {
            p = yi + ((widthMinus1 < i ? widthMinus1 : i) << 2);
            r_sum += (stack.r = pr = pixels[p]) * (rbs = radiusPlus1 - i);
            g_sum += (stack.g = pg = pixels[p + 1]) * rbs;
            b_sum += (stack.b = pb = pixels[p + 2]) * rbs;
            a_sum += (stack.a = pa = pixels[p + 3]) * rbs;

            r_in_sum += pr;
            g_in_sum += pg;
            b_in_sum += pb;
            a_in_sum += pa;

            stack = stack.next;
        }

        stackIn = stackStart;
        stackOut = stackEnd;
        for (x = 0; x < width; x++) {
            pixels[yi + 3] = pa = a_sum * mul_sum >> shg_sum;
            if (pa != 0) {
                pa = 255 / pa;
                pixels[yi] = (r_sum * mul_sum >> shg_sum) * pa;
                pixels[yi + 1] = (g_sum * mul_sum >> shg_sum) * pa;
                pixels[yi + 2] = (b_sum * mul_sum >> shg_sum) * pa;
            } else {
                pixels[yi] = pixels[yi + 1] = pixels[yi + 2] = 0;
            }

            r_sum -= r_out_sum;
            g_sum -= g_out_sum;
            b_sum -= b_out_sum;
            a_sum -= a_out_sum;

            r_out_sum -= stackIn.r;
            g_out_sum -= stackIn.g;
            b_out_sum -= stackIn.b;
            a_out_sum -= stackIn.a;

            p = yw + ((p = x + radius + 1) < widthMinus1 ? p : widthMinus1) << 2;

            r_in_sum += stackIn.r = pixels[p];
            g_in_sum += stackIn.g = pixels[p + 1];
            b_in_sum += stackIn.b = pixels[p + 2];
            a_in_sum += stackIn.a = pixels[p + 3];

            r_sum += r_in_sum;
            g_sum += g_in_sum;
            b_sum += b_in_sum;
            a_sum += a_in_sum;

            stackIn = stackIn.next;

            r_out_sum += pr = stackOut.r;
            g_out_sum += pg = stackOut.g;
            b_out_sum += pb = stackOut.b;
            a_out_sum += pa = stackOut.a;

            r_in_sum -= pr;
            g_in_sum -= pg;
            b_in_sum -= pb;
            a_in_sum -= pa;

            stackOut = stackOut.next;

            yi += 4;
        }
        yw += width;
    }

    for (x = 0; x < width; x++) {
        g_in_sum = b_in_sum = a_in_sum = r_in_sum = g_sum = b_sum = a_sum = r_sum = 0;

        yi = x << 2;
        r_out_sum = radiusPlus1 * (pr = pixels[yi]);
        g_out_sum = radiusPlus1 * (pg = pixels[yi + 1]);
        b_out_sum = radiusPlus1 * (pb = pixels[yi + 2]);
        a_out_sum = radiusPlus1 * (pa = pixels[yi + 3]);

        r_sum += sumFactor * pr;
        g_sum += sumFactor * pg;
        b_sum += sumFactor * pb;
        a_sum += sumFactor * pa;

        stack = stackStart;

        for (i = 0; i < radiusPlus1; i++) {
            stack.r = pr;
            stack.g = pg;
            stack.b = pb;
            stack.a = pa;
            stack = stack.next;
        }

        yp = width;

        for (i = 1; i <= radius; i++) {
            yi = yp + x << 2;

            r_sum += (stack.r = pr = pixels[yi]) * (rbs = radiusPlus1 - i);
            g_sum += (stack.g = pg = pixels[yi + 1]) * rbs;
            b_sum += (stack.b = pb = pixels[yi + 2]) * rbs;
            a_sum += (stack.a = pa = pixels[yi + 3]) * rbs;

            r_in_sum += pr;
            g_in_sum += pg;
            b_in_sum += pb;
            a_in_sum += pa;

            stack = stack.next;

            if (i < heightMinus1) {
                yp += width;
            }
        }

        yi = x;
        stackIn = stackStart;
        stackOut = stackEnd;
        for (y = 0; y < height; y++) {
            p = yi << 2;
            pixels[p + 3] = pa = a_sum * mul_sum >> shg_sum;
            if (pa > 0) {
                pa = 255 / pa;
                pixels[p] = (r_sum * mul_sum >> shg_sum) * pa;
                pixels[p + 1] = (g_sum * mul_sum >> shg_sum) * pa;
                pixels[p + 2] = (b_sum * mul_sum >> shg_sum) * pa;
            } else {
                pixels[p] = pixels[p + 1] = pixels[p + 2] = 0;
            }

            r_sum -= r_out_sum;
            g_sum -= g_out_sum;
            b_sum -= b_out_sum;
            a_sum -= a_out_sum;

            r_out_sum -= stackIn.r;
            g_out_sum -= stackIn.g;
            b_out_sum -= stackIn.b;
            a_out_sum -= stackIn.a;

            p = x + ((p = y + radiusPlus1) < heightMinus1 ? p : heightMinus1) * width << 2;

            r_sum += r_in_sum += stackIn.r = pixels[p];
            g_sum += g_in_sum += stackIn.g = pixels[p + 1];
            b_sum += b_in_sum += stackIn.b = pixels[p + 2];
            a_sum += a_in_sum += stackIn.a = pixels[p + 3];

            stackIn = stackIn.next;

            r_out_sum += pr = stackOut.r;
            g_out_sum += pg = stackOut.g;
            b_out_sum += pb = stackOut.b;
            a_out_sum += pa = stackOut.a;

            r_in_sum -= pr;
            g_in_sum -= pg;
            b_in_sum -= pb;
            a_in_sum -= pa;

            stackOut = stackOut.next;

            yi += width;
        }
    }

    return bitmap;
}

function stackBlurCanvasRGBA(bitmap, top_x, top_y, radius) {
    if (isNaN(radius) || radius < 1) return bitmap;
    radius |= 0;

    var pixels = bitmap.pixels,
        width = bitmap.width,
        height = bitmap.height;

    var x, y, i, p, yp, yi, yw, r_sum, g_sum, b_sum, r_out_sum, g_out_sum, b_out_sum, r_in_sum, g_in_sum, b_in_sum, pr, pg, pb, rbs;

    var div = radius + radius + 1;
    var widthMinus1 = width - 1;
    var heightMinus1 = height - 1;
    var radiusPlus1 = radius + 1;
    var sumFactor = radiusPlus1 * (radiusPlus1 + 1) / 2;

    var stackStart = new BlurStack();
    var stack = stackStart;
    for (i = 1; i < div; i++) {
        stack = stack.next = new BlurStack();
        if (i == radiusPlus1) var stackEnd = stack;
    }
    stack.next = stackStart;
    var stackIn = null;
    var stackOut = null;

    yw = yi = 0;

    var mul_sum = mul_table[radius];
    var shg_sum = shg_table[radius];

    for (y = 0; y < height; y++) {
        r_in_sum = g_in_sum = b_in_sum = r_sum = g_sum = b_sum = 0;

        r_out_sum = radiusPlus1 * (pr = pixels[yi]);
        g_out_sum = radiusPlus1 * (pg = pixels[yi + 1]);
        b_out_sum = radiusPlus1 * (pb = pixels[yi + 2]);

        r_sum += sumFactor * pr;
        g_sum += sumFactor * pg;
        b_sum += sumFactor * pb;

        stack = stackStart;

        for (i = 0; i < radiusPlus1; i++) {
            stack.r = pr;
            stack.g = pg;
            stack.b = pb;
            stack = stack.next;
        }

        for (i = 1; i < radiusPlus1; i++) {
            p = yi + ((widthMinus1 < i ? widthMinus1 : i) << 2);
            r_sum += (stack.r = pr = pixels[p]) * (rbs = radiusPlus1 - i);
            g_sum += (stack.g = pg = pixels[p + 1]) * rbs;
            b_sum += (stack.b = pb = pixels[p + 2]) * rbs;

            r_in_sum += pr;
            g_in_sum += pg;
            b_in_sum += pb;

            stack = stack.next;
        }

        stackIn = stackStart;
        stackOut = stackEnd;
        for (x = 0; x < width; x++) {
            pixels[yi] = r_sum * mul_sum >> shg_sum;
            pixels[yi + 1] = g_sum * mul_sum >> shg_sum;
            pixels[yi + 2] = b_sum * mul_sum >> shg_sum;

            r_sum -= r_out_sum;
            g_sum -= g_out_sum;
            b_sum -= b_out_sum;

            r_out_sum -= stackIn.r;
            g_out_sum -= stackIn.g;
            b_out_sum -= stackIn.b;

            p = yw + ((p = x + radius + 1) < widthMinus1 ? p : widthMinus1) << 2;

            r_in_sum += stackIn.r = pixels[p];
            g_in_sum += stackIn.g = pixels[p + 1];
            b_in_sum += stackIn.b = pixels[p + 2];

            r_sum += r_in_sum;
            g_sum += g_in_sum;
            b_sum += b_in_sum;

            stackIn = stackIn.next;

            r_out_sum += pr = stackOut.r;
            g_out_sum += pg = stackOut.g;
            b_out_sum += pb = stackOut.b;

            r_in_sum -= pr;
            g_in_sum -= pg;
            b_in_sum -= pb;

            stackOut = stackOut.next;

            yi += 4;
        }
        yw += width;
    }

    for (x = 0; x < width; x++) {
        g_in_sum = b_in_sum = r_in_sum = g_sum = b_sum = r_sum = 0;

        yi = x << 2;
        r_out_sum = radiusPlus1 * (pr = pixels[yi]);
        g_out_sum = radiusPlus1 * (pg = pixels[yi + 1]);
        b_out_sum = radiusPlus1 * (pb = pixels[yi + 2]);

        r_sum += sumFactor * pr;
        g_sum += sumFactor * pg;
        b_sum += sumFactor * pb;

        stack = stackStart;

        for (i = 0; i < radiusPlus1; i++) {
            stack.r = pr;
            stack.g = pg;
            stack.b = pb;
            stack = stack.next;
        }

        yp = width;

        for (i = 1; i <= radius; i++) {
            yi = yp + x << 2;

            r_sum += (stack.r = pr = pixels[yi]) * (rbs = radiusPlus1 - i);
            g_sum += (stack.g = pg = pixels[yi + 1]) * rbs;
            b_sum += (stack.b = pb = pixels[yi + 2]) * rbs;

            r_in_sum += pr;
            g_in_sum += pg;
            b_in_sum += pb;

            stack = stack.next;

            if (i < heightMinus1) {
                yp += width;
            }
        }

        yi = x;
        stackIn = stackStart;
        stackOut = stackEnd;
        for (y = 0; y < height; y++) {
            p = yi << 2;
            pixels[p] = r_sum * mul_sum >> shg_sum;
            pixels[p + 1] = g_sum * mul_sum >> shg_sum;
            pixels[p + 2] = b_sum * mul_sum >> shg_sum;

            r_sum -= r_out_sum;
            g_sum -= g_out_sum;
            b_sum -= b_out_sum;

            r_out_sum -= stackIn.r;
            g_out_sum -= stackIn.g;
            b_out_sum -= stackIn.b;

            p = x + ((p = y + radiusPlus1) < heightMinus1 ? p : heightMinus1) * width << 2;

            r_sum += r_in_sum += stackIn.r = pixels[p];
            g_sum += g_in_sum += stackIn.g = pixels[p + 1];
            b_sum += b_in_sum += stackIn.b = pixels[p + 2];

            stackIn = stackIn.next;

            r_out_sum += pr = stackOut.r;
            g_out_sum += pg = stackOut.g;
            b_out_sum += pb = stackOut.b;

            r_in_sum -= pr;
            g_in_sum -= pg;
            b_in_sum -= pb;

            stackOut = stackOut.next;

            yi += width;
        }
    }

    return bitmap;
}

function stackBlur () {
    var radius = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 10;
    var hasAlphaChannel = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : true;

    radius = parseParamNumber(radius);

    return function (bitmap, done) {
        var newBitmap = stackBlurImage(bitmap, radius, hasAlphaChannel);

        done(newBitmap);
    };
}

function transparency() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 100;

    amount = parseParamNumber(amount);
    return convolution(weight([1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0.3, 0, 0, 0, 0, 0, 1], amount / 100));
}

function unsharpMasking() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 256;

    amount = parseParamNumber(amount);
    return convolution(weight([1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, -476, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4, 1], -1 / amount));
}

var matrix$1 = {
     blur: blur,
     emboss: emboss,
     gaussianBlur: gaussianBlur,
     'gaussian-blur': gaussianBlur,
     gaussianBlur5x: gaussianBlur5x,
     'gaussian-blur-5x': gaussianBlur5x,
     grayscale2: grayscale2,
     normal: identity,
     kirschHorizontal: kirschHorizontal,
     'kirsch-horizontal': kirschHorizontal,
     kirschVertical: kirschVertical,
     'kirsch-vertical': kirschVertical,
     laplacian: laplacian,
     laplacian5x: laplacian5x,
     'laplacian-5x': laplacian5x,
     motionBlur: motionBlur,
     'motion-blur': motionBlur,
     motionBlur2: motionBlur2,
     'motion-blur-2': motionBlur2,
     motionBlur3: motionBlur3,
     'motion-blur-3': motionBlur3,
     negative: negative,
     sepia2: sepia2,
     sharpen: sharpen,
     sobelHorizontal: sobelHorizontal,
     'sobel-horizontal': sobelHorizontal,
     sobelVertical: sobelVertical,
     'sobel-vertical': sobelVertical,
     stackBlur: stackBlur,
     'stack-blur': stackBlur,
     transparency: transparency,
     unsharpMasking: unsharpMasking,
     'unsharp-masking': unsharpMasking
};

function kirsch() {
    return filter$1('kirsch-horizontal kirsch-vertical');
}

function sobel() {
    return filter$1('sobel-horizontal sobel-vertical');
}

function vintage() {
    return filter$1('brightness(15) saturation(-20) gamma(1.8)');
}

var multi$2 = {
    kirsch: kirsch,
    sobel: sobel,
    vintage: vintage
};

var FilterList = _extends({}, image$1, pixel$1, matrix$1, multi$2);

var _functions;

var makeId = 0;

var functions$1 = (_functions = {
    partial: partial,
    multi: multi$1,
    merge: merge$1,
    weight: weight,
    repeat: repeat,
    colorMatrix: colorMatrix,
    each: each$1,
    eachXY: eachXY,
    createRandomCount: createRandomCount,
    createRandRange: createRandRange,
    createBitmap: createBitmap,
    createBlurMatrix: createBlurMatrix,
    pack: pack$1,
    packXY: packXY,
    pixel: pixel,
    getBitmap: getBitmap,
    putBitmap: putBitmap,
    radian: radian,
    convolution: convolution,
    parseParamNumber: parseParamNumber,
    filter: filter$1,
    clamp: clamp$1,
    fillColor: fillColor,
    fillPixelColor: fillPixelColor
}, defineProperty(_functions, 'multi', multi$1), defineProperty(_functions, 'merge', merge$1), defineProperty(_functions, 'matches', matches$1), defineProperty(_functions, 'parseFilter', parseFilter), defineProperty(_functions, 'partial', partial), _functions);

var LocalFilter = functions$1;

function weight(arr) {
    var num = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 1;

    return arr.map(function (i) {
        return i * num;
    });
}

function repeat(value, num) {
    var arr = new Array(num);
    for (var i = 0; i < num; i++) {
        arr[i] = value;
    }
    return arr;
}

function colorMatrix(pixels, i, matrix) {
    var r = pixels[i],
        g = pixels[i + 1],
        b = pixels[i + 2],
        a = pixels[i + 3];

    fillColor(pixels, i, matrix[0] * r + matrix[1] * g + matrix[2] * b + matrix[3] * a, matrix[4] * r + matrix[5] * g + matrix[6] * b + matrix[7] * a, matrix[8] * r + matrix[9] * g + matrix[10] * b + matrix[11] * a, matrix[12] * r + matrix[13] * g + matrix[14] * b + matrix[15] * a);
}

function makeFilter$1(filter) {

    if (typeof filter == 'function') {
        return filter;
    }

    if (typeof filter == 'string') {
        filter = [filter];
    }

    filter = filter.slice(0);
    var filterName = filter.shift();

    if (typeof filterName == 'function') {
        return filterName;
    }

    var params = filter;

    var filterFunction = FilterList[filterName] || LocalFilter[filterName];

    if (!filterFunction) {
        throw new Error(filterName + ' is not filter. please check filter name.');
    }
    return filterFunction.apply(filterFunction, params);
}

function forLoop(max) {
    var index = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
    var step = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 1;
    var callback = arguments[3];
    var done = arguments[4];
    var functionDumpCount = arguments.length > 5 && arguments[5] !== undefined ? arguments[5] : 10000;
    var frameTimer = arguments.length > 6 && arguments[6] !== undefined ? arguments[6] : 'full';
    var loopCount = arguments.length > 7 && arguments[7] !== undefined ? arguments[7] : 50;

    var runIndex = index;
    var timer = function timer(callback) {
        setTimeout(callback, 0);
    };

    if (frameTimer == 'requestAnimationFrame') {
        timer = requestAnimationFrame;
        functionDumpCount = 1000;
    }

    if (frameTimer == 'full') {
        /* only for loop  */
        timer = null;
        functionDumpCount = max;
    }

    function makeFunction() {
        var count = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 50;

        var arr = [].concat(toConsumableArray(Array(count)));

        var functionStrings = arr.map(function (countIndex) {
            return 'cri = ri + i * s; if (cri >= mx) return {currentRunIndex: cri, i: null}; c(cri); i++;';
        }).join('\n');

        var smallLoopFunction = new Function('ri', 'i', 's', 'mx', 'c', '\n            let cri = ri;\n            \n            ' + functionStrings + '\n            \n            return {currentRunIndex: cri, i: i} \n        ');

        return smallLoopFunction;
    }

    function runCallback() {

        var smallLoopFunction = makeFunction(loopCount); // loop is call  20 callbacks at once 

        var currentRunIndex = runIndex;
        var ret = {};
        var i = 0;
        while (i < functionDumpCount) {
            ret = smallLoopFunction(runIndex, i, step, max, callback);

            if (ret.i == null) {
                currentRunIndex = ret.currentRunIndex;
                break;
            }

            i = ret.i;
            currentRunIndex = ret.currentRunIndex;
        }

        nextCallback(currentRunIndex);
    }

    function nextCallback(currentRunIndex) {
        if (currentRunIndex) {
            runIndex = currentRunIndex;
        } else {
            runIndex += step;
        }

        if (runIndex >= max) {
            done();
            return;
        }

        if (timer) timer(runCallback);else runCallback();
    }

    runCallback();
}

function each$1(len, callback, done) {
    var opt = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : {};


    forLoop(len, 0, 4, function (i) {
        callback(i, i >> 2 /* xyIndex */);
    }, function () {
        done();
    }, opt.functionDumpCount, opt.frameTimer, opt.loopCount);
}

function eachXY(len, width, callback, done) {
    var opt = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : {};


    forLoop(len, 0, 4, function (i) {
        var xyIndex = i >> 2;
        callback(i, xyIndex % width, Math.floor(xyIndex / width));
    }, function () {
        done();
    }, opt.functionDumpCount, opt.frameTimer, opt.loopCount);
}

function createRandRange(min, max, count) {
    var result = [];

    for (var i = 1; i <= count; i++) {
        var num = Math.random() * (max - min) + min;
        var sign = Math.floor(Math.random() * 10) % 2 == 0 ? -1 : 1;
        result.push(sign * num);
    }

    result.sort();

    var centerIndex = Math.floor(count >> 1);
    var a = result[centerIndex];
    result[centerIndex] = result[0];
    result[0] = a;

    return result;
}

function createRandomCount() {
    return [3 * 3, 4 * 4, 5 * 5, 6 * 6, 7 * 7, 8 * 8, 9 * 9, 10 * 10].sort(function (a, b) {
        return 0.5 - Math.random();
    })[0];
}

function createBitmap(length, width, height) {
    return { pixels: new Uint8ClampedArray(length), width: width, height: height };
}

function putPixel(dstBitmap, srcBitmap, startX, startY) {

    var len = srcBitmap.pixels.length / 4;
    var dstX = 0,
        dstY = 0,
        x = 0,
        y = 0,
        srcIndex = 0,
        dstIndex = 0;
    for (var i = 0; i < len; i++) {
        x = i % srcBitmap.width, y = Math.floor(i / srcBitmap.width);

        dstX = startX + x;
        dstY = startY + y;

        if (dstX > dstBitmap.width) continue;
        if (dstY > dstBitmap.height) continue;

        srcIndex = y * srcBitmap.width + x << 2;
        dstIndex = dstY * dstBitmap.width + dstX << 2;

        dstBitmap.pixels[dstIndex] = srcBitmap.pixels[srcIndex];
        dstBitmap.pixels[dstIndex + 1] = srcBitmap.pixels[srcIndex + 1];
        dstBitmap.pixels[dstIndex + 2] = srcBitmap.pixels[srcIndex + 2];
        dstBitmap.pixels[dstIndex + 3] = srcBitmap.pixels[srcIndex + 3];
    }
}

function getPixel(srcBitmap, dstBitmap, startX, startY) {
    var len = dstBitmap.pixels.length >> 2;
    var srcX = 0,
        srcY = 0,
        x = 0,
        y = 0,
        srcIndex = 0,
        dstIndex = 0;
    for (var i = 0; i < len; i++) {
        var x = i % dstBitmap.width,
            y = Math.floor(i / dstBitmap.width);

        srcX = startX + x;
        srcY = startY + y;

        if (srcX > srcBitmap.width) continue;
        if (srcY > srcBitmap.height) continue;

        srcIndex = srcY * srcBitmap.width + srcX << 2;
        dstIndex = y * dstBitmap.width + x << 2;

        dstBitmap.pixels[dstIndex] = srcBitmap.pixels[srcIndex];
        dstBitmap.pixels[dstIndex + 1] = srcBitmap.pixels[srcIndex + 1];
        dstBitmap.pixels[dstIndex + 2] = srcBitmap.pixels[srcIndex + 2];
        dstBitmap.pixels[dstIndex + 3] = srcBitmap.pixels[srcIndex + 3];
    }
}

function cloneBitmap(bitmap) {
    var padding = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;


    var width = bitmap.width + padding;
    var height = bitmap.height + padding;

    var newBitmap = { pixels: new Uint8ClampedArray(width * height * 4), width: width, height: height };

    return newBitmap;
}

function getBitmap(bitmap, area) {
    return Canvas.getBitmap(bitmap, area);
}

function putBitmap(bitmap, subBitmap, area) {
    return Canvas.putBitmap(bitmap, subBitmap, area);
}

function parseParamNumber(param) {
    if (typeof param === 'string') {
        param = param.replace(/deg/, '');
        param = param.replace(/px/, '');
    }
    return +param;
}

var filter_regexp = /(([\w_\-]+)(\(([^\)]*)\))?)+/gi;
function pack$1(callback) {
    return function (bitmap, done) {
        each$1(bitmap.pixels.length, function (i, xyIndex) {
            callback(bitmap.pixels, i, xyIndex, bitmap.pixels[i], bitmap.pixels[i + 1], bitmap.pixels[i + 2], bitmap.pixels[i + 3]);
        }, function () {
            done(bitmap);
        });
    };
}

function makePrebuildUserFilterList(arr) {

    var codeString = arr.map(function (it) {
        return ' \n            ' + it.userFunction.$preContext + '\n\n            ' + it.userFunction.$preCallbackString + '\n\n            $r = clamp($r); $g = clamp($g); $b = clamp($b); $a = clamp($a);\n        ';
    }).join('\n\n');

    var rootContextObject = { clamp: clamp$1, Color: Color$1 };
    arr.forEach(function (it) {
        Object.assign(rootContextObject, it.userFunction.rootContextObject);
    });

    var rootContextDefine = 'const ' + Object.keys(rootContextObject).map(function (key) {
        return ' ' + key + ' = $rc.' + key + ' ';
    }).join(',');

    var FunctionCode = ' \n    let $r = $p[$pi], $g = $p[$pi+1], $b = $p[$pi+2], $a = $p[$pi+3];\n    \n    ' + rootContextDefine + '\n\n    ' + codeString + '\n    \n    $p[$pi] = $r; $p[$pi+1] = $g; $p[$pi+2] = $b; $p[$pi+3] = $a;\n    ';

    var userFunction = new Function('$p', '$pi', '$rc', FunctionCode);

    return function ($pixels, $pixelIndex) {
        userFunction($pixels, $pixelIndex, rootContextObject);
    };
}

function makeUserFilterFunctionList(arr) {
    var rootContextObject = {};
    var list = arr.map(function (it) {
        var newKeys = [];

        Object.keys(it.context).forEach(function (key, i) {
            newKeys[key] = 'n$' + makeId++ + key + '$';
        });

        Object.keys(it.rootContext).forEach(function (key, i) {
            newKeys[key] = 'r$' + makeId++ + key + '$';

            rootContextObject[newKeys[key]] = it.rootContext[key];
        });

        var preContext = Object.keys(it.context).filter(function (key) {
            if (typeof it.context[key] === 'number' || typeof it.context[key] === 'string') {
                return false;
            } else if (Array.isArray(it.context[key])) {
                if (typeof it.context[key][0] == 'number' || typeof it.context[key][0] == 'string') {
                    return false;
                }
            }

            return true;
        }).map(function (key, i) {
            return [newKeys[key], JSON.stringify(it.context[key])].join(' = ');
        });

        var preCallbackString = it.callback;

        if (typeof it.callback === 'function') {
            preCallbackString = it.callback.toString().split("{");

            preCallbackString.shift();
            preCallbackString = preCallbackString.join("{");
            preCallbackString = preCallbackString.split("}");
            preCallbackString.pop();
            preCallbackString = preCallbackString.join("}");
        }

        Object.keys(newKeys).forEach(function (key) {
            var newKey = newKeys[key];

            if (typeof it.context[key] === 'number' || typeof it.context[key] === 'string') {
                preCallbackString = preCallbackString.replace(new RegExp("\\" + key, "g"), it.context[key]);
            } else if (Array.isArray(it.context[key])) {
                if (typeof it.context[key][0] == 'number' || typeof it.context[key][0] == 'string') {
                    it.context[key].forEach(function (item, index) {
                        preCallbackString = preCallbackString.replace(new RegExp("\\" + key + '\\[' + index + '\\]', "g"), item);
                    });
                } else {
                    preCallbackString = preCallbackString.replace(new RegExp("\\" + key, "g"), newKey);
                }
            } else {
                preCallbackString = preCallbackString.replace(new RegExp("\\" + key, "g"), newKey);
            }
        });

        return { preCallbackString: preCallbackString, preContext: preContext };
    });

    var preContext = list.map(function (it, i) {
        return it.preContext.length ? 'const ' + it.preContext + ';' : "";
    }).join('\n\n');

    var preCallbackString = list.map(function (it) {
        return it.preCallbackString;
    }).join('\n\n');

    var FunctionCode = ' \n    let $r = $pixels[$pixelIndex], $g = $pixels[$pixelIndex+1], $b = $pixels[$pixelIndex+2], $a = $pixels[$pixelIndex+3];\n\n    ' + preContext + '\n\n    ' + preCallbackString + '\n    \n    $pixels[$pixelIndex] = $r\n    $pixels[$pixelIndex+1] = $g \n    $pixels[$pixelIndex+2] = $b   \n    $pixels[$pixelIndex+3] = $a   \n    ';

    var userFunction = new Function('$pixels', '$pixelIndex', '$clamp', '$Color', FunctionCode);

    userFunction.$preCallbackString = preCallbackString;
    userFunction.$preContext = preContext;
    userFunction.rootContextObject = rootContextObject;

    return userFunction;
}

function makeUserFilterFunction(callback) {
    var context = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
    var rootContext = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};

    return makeUserFilterFunctionList([{ callback: callback, context: context, rootContext: rootContext }]);
}

function pixel(callback) {
    var context = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
    var rootContext = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};

    var userFunction = makeUserFilterFunction(callback, context, rootContext);

    var returnCallback = function returnCallback(bitmap, done) {};

    returnCallback.userFunction = userFunction;

    return returnCallback;
}

var ColorListIndex = [0, 1, 2, 3];

function swapColor(pixels, startIndex, endIndex) {

    ColorListIndex.forEach(function (i) {
        var temp = pixels[startIndex + i];
        pixels[startIndex + i] = pixels[endIndex + i];
        pixels[endIndex + i] = temp;
    });
}

function packXY(callback) {
    return function (bitmap, done) {
        var opt = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};

        eachXY(bitmap.pixels.length, bitmap.width, function (i, x, y) {
            callback(bitmap.pixels, i, x, y);
        }, function () {
            done(bitmap);
        }, opt);
    };
}

function radian(degree) {
    return Matrix.CONSTANT.radian(degree);
}

function createBlurMatrix() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 3;

    var count = Math.pow(amount, 2);
    var value = 1 / count;
    return repeat(value, count);
}

function fillColor(pixels, i, r, g, b, a) {
    if (arguments.length == 3) {
        var _arguments$ = arguments[2],
            r = _arguments$.r,
            g = _arguments$.g,
            b = _arguments$.b,
            a = _arguments$.a;
    }

    if (typeof r == 'number') {
        pixels[i] = r;
    }
    if (typeof g == 'number') {
        pixels[i + 1] = g;
    }
    if (typeof b == 'number') {
        pixels[i + 2] = b;
    }
    if (typeof a == 'number') {
        pixels[i + 3] = a;
    }
}

function fillPixelColor(targetPixels, targetIndex, sourcePixels, sourceIndex) {
    fillColor(targetPixels, targetIndex, sourcePixels[sourceIndex], sourcePixels[sourceIndex + 1], sourcePixels[sourceIndex + 2], sourcePixels[sourceIndex + 3]);
}



function createWeightTable(weights) {
    var min = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
    var max = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 255;

    var weightTable = [];

    weightTable = weights.map(function (w, i) {
        return [];
    });

    weights.forEach(function (w, i) {

        if (w != 0) {
            var data = weightTable[i];

            for (var i = min; i <= max; i++) {
                data[i] = w * i;
            }
        }
    });

    return weightTable;
}

function createSubPixelWeightFunction(weights, weightTable, width, height, opaque) {

    var side = Math.round(Math.sqrt(weights.length));
    var halfSide = Math.floor(side / 2);
    var alphaFac = opaque ? 1 : 0;

    var FunctionCode = 'let r = 0, g = 0, b = 0, a = 0, scy = 0, scx =0, si = 0; ';
    var R = [];
    var G = [];
    var B = [];
    var A = [];
    weights.forEach(function (wt, index) {
        var cy = Math.floor(index / side);
        var cx = index % side;
        var distY = cy - halfSide;
        var distX = cx - halfSide;

        if (wt == 0) {
            return;
        }

        R.push('$t[' + index + '][$sp[(($sy + (' + distY + ')) * ' + width + ' + ($sx + (' + distX + '))) * 4]]');
        G.push('$t[' + index + '][$sp[(($sy + (' + distY + ')) * ' + width + ' + ($sx + (' + distX + '))) * 4 + 1]]');
        B.push('$t[' + index + '][$sp[(($sy + (' + distY + ')) * ' + width + ' + ($sx + (' + distX + '))) * 4 + 2]]');
        A.push('$t[' + index + '][$sp[(($sy + (' + distY + ')) * ' + width + ' + ($sx + (' + distX + '))) * 4 + 3]]');
    });

    FunctionCode += 'r = ' + R.join(' + ') + '; g = ' + G.join(' + ') + '; b = ' + B.join(' + ') + '; a = ' + A.join(' + ') + ';';
    FunctionCode += '$dp[$di] = r; $dp[$di+1] = g;$dp[$di+2] = b;$dp[$di+3] = a + (' + alphaFac + ')*(255-a); ';

    // console.log(FunctionCode)

    var subPixelFunction = new Function('$dp', '$sp', '$di', '$sx', '$sy', '$t', FunctionCode);

    return function ($dp, $sp, $di, $sx, $sy) {
        subPixelFunction($dp, $sp, $di, $sx, $sy, weightTable);
    };
}

function convolution(weights) {
    var opaque = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : true;

    var weightTable = createWeightTable(weights);
    return function (bitmap, done) {
        var side = Math.round(Math.sqrt(weights.length));
        var padding = side * 2;

        // 원본 크기를 늘림 
        var sourceBitmap = cloneBitmap(bitmap, padding);

        // 원본 데이타 복사 
        putPixel(sourceBitmap, bitmap, side, side);

        // 최종 아웃풋 
        var newBitmap = createBitmap(sourceBitmap.pixels.length, sourceBitmap.width, sourceBitmap.height);

        // 마지막 원본 아웃풋 
        var returnBitmap = createBitmap(bitmap.pixels.length, bitmap.width, bitmap.height);

        var subPixelWeightFunction = createSubPixelWeightFunction(weights, weightTable, sourceBitmap.width, sourceBitmap.height, opaque);

        var len = bitmap.pixels.length / 4;
        for (var i = 0; i < len; i++) {
            var xyIndex = i,
                x = xyIndex % bitmap.width + side,
                y = Math.floor(xyIndex / bitmap.width) + side;

            subPixelWeightFunction(newBitmap.pixels, sourceBitmap.pixels, (y * sourceBitmap.width + x) * 4, x, y);
        }

        getPixel(newBitmap, returnBitmap, side, side);
        done(returnBitmap);
    };
}

function matches$1(str) {
    var ret = Color$1.convertMatches(str);
    var matches = ret.str.match(filter_regexp);
    var result = [];

    if (!matches) {
        return result;
    }

    result = matches.map(function (it) {
        return { filter: it, origin: Color$1.reverseMatches(it, ret.matches) };
    });

    var pos = { next: 0 };
    result = result.map(function (item) {

        var startIndex = str.indexOf(item.origin, pos.next);

        item.startIndex = startIndex;
        item.endIndex = startIndex + item.origin.length;

        item.arr = parseFilter(item.origin);

        pos.next = item.endIndex;

        return item;
    }).filter(function (it) {
        if (!it.arr.length) return false;
        return true;
    });

    return result;
}

/**
 * Filter Parser 
 * 
 * F.parseFilter('blur(30)') == ['blue', '30']
 * F.parseFilter('gradient(white, black, 3)') == ['gradient', 'white', 'black', '3']
 * 
 * @param {String} filterString 
 */
function parseFilter(filterString) {

    var ret = Color$1.convertMatches(filterString);
    var matches = ret.str.match(filter_regexp);

    if (!matches[0]) {
        return [];
    }

    var arr = matches[0].split('(');

    var filterName = arr.shift();
    var filterParams = [];

    if (arr.length) {
        filterParams = arr.shift().split(')')[0].split(',').map(function (f) {
            return Color$1.reverseMatches(f, ret.matches);
        });
    }

    var result = [filterName].concat(toConsumableArray(filterParams)).map(Color$1.trim);

    return result;
}

function clamp$1(num) {
    return Math.min(255, num);
}

function filter$1(str) {
    return merge$1(matches$1(str).map(function (it) {
        return it.arr;
    }));
}

function makeGroupedFilter$1() {
    var filters = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : [];

    var groupedFilter = [];
    var group = [];
    for (var i = 0, len = filters.length; i < len; i++) {
        var f = filters[i];

        if (f.userFunction) {
            group.push(f);
        } else {
            if (group.length) {
                groupedFilter.push([].concat(toConsumableArray(group)));
            }
            groupedFilter.push(f);
            group = [];
        }
    }

    if (group.length) {
        groupedFilter.push([].concat(toConsumableArray(group)));
    }

    groupedFilter.forEach(function (filter, index) {
        if (Array.isArray(filter)) {
            groupedFilter[index] = function () {
                var userFunction = makePrebuildUserFilterList(filter);
                // console.log(userFunction)
                return function (bitmap, done) {

                    for (var i = 0, len = bitmap.pixels.length; i < len; i += 4) {
                        userFunction(bitmap.pixels, i);
                    }

                    done(bitmap);
                    // forLoop(bitmap.pixels.length, 0, 4, function (i) {
                    //     userFunction(bitmap.pixels, i)
                    // }, function () {
                    //     done(bitmap)
                    // })
                };
            }();
        }
    });

    return groupedFilter;
}

/** 
 * 
 * multiply filters
 * 
 * ImageFilter.multi('blur', 'grayscale', 'sharpen', ['blur', 3], function (bitmap) {  return bitmap });
 * 
 */
function multi$1() {
    for (var _len = arguments.length, filters = Array(_len), _key = 0; _key < _len; _key++) {
        filters[_key] = arguments[_key];
    }

    filters = filters.map(function (filter) {
        return makeFilter$1(filter);
    }).filter(function (f) {
        return f;
    });

    filters = makeGroupedFilter$1(filters);

    var max = filters.length;

    return function (bitmap, done) {
        var opt = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};


        var currentBitmap = bitmap;
        var index = 0;

        function runFilter() {
            filters[index].call(null, currentBitmap, function (nextBitmap) {
                currentBitmap = nextBitmap;

                nextFilter();
            }, opt);
        }

        function nextFilter() {
            index++;

            if (index >= max) {
                done(currentBitmap);
                return;
            }

            runFilter();
        }

        runFilter();
    };
}

function merge$1(filters) {
    return multi$1.apply(undefined, toConsumableArray(filters));
}

/**
 * apply filter into special area
 * 
 * F.partial({x,y,width,height}, filter, filter, filter )
 * F.partial({x,y,width,height}, 'filter' )
 * 
 * @param {{x, y, width, height}} area 
 * @param {*} filters   
 */
function partial(area) {
    var allFilter = null;

    for (var _len2 = arguments.length, filters = Array(_len2 > 1 ? _len2 - 1 : 0), _key2 = 1; _key2 < _len2; _key2++) {
        filters[_key2 - 1] = arguments[_key2];
    }

    if (filters.length == 1 && typeof filters[0] === 'string') {
        allFilter = filter$1(filters[0]);
    } else {
        allFilter = merge$1(filters);
    }

    return function (bitmap, done) {
        var opt = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};

        allFilter(getBitmap(bitmap, area), function (newBitmap) {
            done(putBitmap(bitmap, newBitmap, area));
        }, opt);
    };
}

function parseParamNumber$1(param) {
    if (typeof param === 'string') {
        param = param.replace(/deg/, '');
        param = param.replace(/px/, '');
    }
    return +param;
}

function weight$1(arr) {
    var num = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 1;

    return arr.map(function (i) {
        return i * num;
    });
}

var SHADER_INDEX = 0;

function convolutionString(count) {

    var width = Math.sqrt(count);
    var half = Math.floor(width / 2);

    return [].concat(toConsumableArray(Array(count))).map(function (it, index) {
        var y = Math.floor(index / width) - half;
        var x = index % width - half;

        return 'texture(u_image, v_texCoord + onePixel * vec2(' + x + ', ' + y + ')) * u_kernel' + count + '[' + index + ']';
    }).join(' + \n');
}

function multi$3(str) {
    return [].concat(Array.prototype.slice.call(arguments));
}

function convolution$1(arr) {

    return {
        type: 'convolution',
        length: arr.length,
        content: arr
    };
}

function makeShader(str, index) {
    return '\n        if (u_filterIndex == ' + index + '.0) {\n            ' + str + '\n        }\n    ';
}

function shader(str, options) {
    return {
        type: 'shader',
        index: SHADER_INDEX,
        options: options,
        content: makeShader(str, SHADER_INDEX++)
    };
}

function makeVertexShaderSource() {
    return '#version 300 es \n\n        in vec2 a_position;\n        in vec2 a_texCoord; \n\n        uniform vec2 u_resolution;\n        uniform float u_flipY;\n\n        out vec2 v_texCoord; \n\n        void main() {\n            vec2 zeroToOne = a_position / u_resolution;\n\n            vec2 zeroToTwo = zeroToOne * 2.0;\n\n            vec2 clipSpace = zeroToTwo - 1.0;\n\n            gl_Position = vec4(clipSpace * vec2(1, u_flipY), 0, 1);\n\n            v_texCoord = a_texCoord;\n\n        }\n    ';
}

function makeConvolution(count) {

    return '\n    \n    if (u_kernelSelect == ' + count + '.0) {\n        vec4 colorSum = ' + convolutionString(count) + '; \n\n        outColor = vec4((colorSum / u_kernel' + count + 'Weight).rgb, 1);\n        \n    }\n    ';
}

function makeFragmentShaderSource(filterShaderList) {

    var filterContent = filterShaderList.filter(function (f) {
        return f.type == 'shader';
    }).map(function (f) {
        return f.content;
    }).join('\n\n');

    var weightTable = { '9': true };

    filterShaderList.filter(function (f) {
        return f.type == 'convolution';
    }).forEach(function (f) {
        weightTable[f.length] = true;
    });

    var convolutionString = Object.keys(weightTable).map(function (len) {
        return makeConvolution(+len);
    }).join('\n');

    return '#version 300 es\n\n    precision highp int;\n    precision mediump float;\n    \n    uniform sampler2D u_image;\n\n    // 3 is 3x3 matrix kernel \n    uniform float u_kernelSelect;\n    uniform float u_filterIndex;\n\n    uniform float u_kernel9[9];\n    uniform float u_kernel9Weight;\n    uniform float u_kernel25[25];\n    uniform float u_kernel25Weight;\n    uniform float u_kernel49[49];\n    uniform float u_kernel49Weight;\n    uniform float u_kernel81[81];\n    uniform float u_kernel81Weight;    \n\n    in vec2 v_texCoord;\n    \n    out vec4 outColor;\n\n    float random (vec2 st) {\n        return fract(sin(dot(st.xy, vec2(12.9898,78.233)))* 43758.5453123);\n    } \n\n    // \n    vec3 rgb2hsv(vec3 c)\n    {\n        vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);\n        vec4 p = c.g < c.b ? vec4(c.bg, K.wz) : vec4(c.gb, K.xy);\n        vec4 q = c.r < p.x ? vec4(p.xyw, c.r) : vec4(c.r, p.yzx);\n\n        float d = q.x - min(q.w, q.y);\n        float e = 1.0e-10;\n        return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);\n    }\n\n    vec3 hsv2rgb(vec3 c)\n    {\n        vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);\n        vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n        return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n    }\n    \n    void main() {\n        vec4 pixelColor = texture(u_image, v_texCoord);\n        vec2 onePixel = vec2(1) / vec2(textureSize(u_image, 0));                \n\n        ' + filterContent + '\n\n        ' + convolutionString + '\n\n    }';
}

function colorToVec4(color) {
    color = [color.r / 255, color.g / 255, color.b / 255, color.a || 0].map(toFloatString);
    return 'vec4(' + color + ')';
}

function toFloatString(number) {
    if (number == Math.floor(number)) {
        return number + '.0';
    }

    return number;
}

function blur$1 () {
    return convolution$1([1, 1, 1, 1, 1, 1, 1, 1, 1]);
}

function normal () {
    return convolution$1([0, 0, 0, 0, 1, 0, 0, 0, 0]);
}

/*
 * carve, mold, or stamp a design on (a surface) so that it stands out in relief.
 * 
 * @param {Number} amount   0.0 .. 4.0 
 */
function emboss$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 4;

    amount = parseParamNumber$1(amount);
    return convolution$1([amount * -2.0, -amount, 0.0, -amount, 1.0, amount, 0.0, amount, amount * 2.0]);
}

/**
 * 
 * @param {Number} amount 0..1
 */
function gaussianBlur$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var C = parseParamNumber$1(amount) * (1 / 16);

    return convolution$1(weight$1([1, 2, 1, 2, 4, 2, 1, 2, 1], C));
}

function gaussianBlur5x$1() {
    return convolution$1([1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, 36, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4, 1]);
}

function grayscale2$1() {
    return convolution$1([0.3, 0.3, 0.3, 0, 0, 0.59, 0.59, 0.59, 0, 0, 0.11, 0.11, 0.11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
}

function kirschHorizontal$1() {
    return convolution$1([5, 5, 5, -3, 0, -3, -3, -3, -3]);
}

function kirschVertical$1() {
    return convolution$1([5, -3, -3, 5, 0, -3, 5, -3, -3]);
}

function laplacian$1() {
    return convolution$1([-1, -1, -1, -1, 8, -1, -1, -1, -1]);
}

function laplacian5x$1() {
    return convolution$1([-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 24, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1]);
}

function motionBlur$1() {
    return convolution$1([1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1]);
}

function motionBlur2$1() {
    return convolution$1([1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1]);
}

function motionBlur3$1() {
    return convolution$1([1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1]);
}

function negative$1() {
    return convolution$1([-1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1]);
}

function sepia2$1() {
    return convolution$1([0.393, 0.349, 0.272, 0, 0, 0.769, 0.686, 0.534, 0, 0, 0.189, 0.168, 0.131, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
}

function sharpen$1() {
    return convolution$1([0, -1, 0, -1, 5, -1, 0, -1, 0]);
}

function sobelHorizontal$1() {
    return convolution$1([-1, -2, -1, 0, 0, 0, 1, 2, 1]);
}

function sobelVertical$1() {
    return convolution$1([-1, 0, 1, -2, 0, 2, -1, 0, 1]);
}

function transparency$1() {
    return convolution$1([1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0.3, 0, 0, 0, 0, 0, 1]);
}

function unsharpMasking$1() {
    return convolution$1(weight$1([1, 4, 6, 4, 1, 4, 16, 24, 16, 4, 6, 24, -476, 24, 6, 4, 16, 24, 16, 4, 1, 4, 6, 4, 1], -1 / 256));
}

var matrix$2 = {
     blur: blur$1,
     normal: normal,
     emboss: emboss$1,
     gaussianBlur: gaussianBlur$1,
     'gaussian-blur': gaussianBlur$1,
     gaussianBlur5x: gaussianBlur5x$1,
     'gaussian-blur-5x': gaussianBlur5x$1,
     grayscale2: grayscale2$1,
     kirschHorizontal: kirschHorizontal$1,
     'kirsch-horizontal': kirschHorizontal$1,
     kirschVertical: kirschVertical$1,
     'kirsch-vertical': kirschVertical$1,
     laplacian: laplacian$1,
     laplacian5x: laplacian5x$1,
     'laplacian-5x': laplacian5x$1,
     motionBlur: motionBlur$1,
     'motion-blur': motionBlur$1,
     motionBlur2: motionBlur2$1,
     'motion-blur-2': motionBlur2$1,
     motionBlur3: motionBlur3$1,
     'motion-blur-3': motionBlur3$1,
     negative: negative$1,
     sepia2: sepia2$1,
     sharpen: sharpen$1,
     sobelHorizontal: sobelHorizontal$1,
     'sobel-horizontal': sobelHorizontal$1,
     sobelVertical: sobelVertical$1,
     'sobel-vertical': sobelVertical$1,
     transparency: transparency$1,
     unsharpMasking: unsharpMasking$1,
     'unsharp-masking': unsharpMasking$1
};

function bitonal$1(darkColor, lightColor) {
    var threshold = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0.5;

    var checkVlue = toFloatString(threshold);
    var darkColorString = colorToVec4(Color$1.parse(darkColor));
    var lightColorString = colorToVec4(Color$1.parse(lightColor));

    return shader('\n        if ((pixelColor.r + pixelColor.g + pixelColor.b) > ' + checkVlue + ') {\n            outColor = vec4(' + lightColorString + '.rgb, pixelColor.a);\n        } else {\n            outColor = vec4(' + darkColorString + '.rgb, pixelColor.a);\n        }\n    ');
}

/*
 * @param {Number} amount  -1..1  ,  value < 0  is darken, value > 0 is brighten 
 */
function brightness$2() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var C = toFloatString(parseParamNumber$1(amount));

    return shader('\n        outColor = pixelColor + (' + C + ');\n    ');
}

function matrix$3() {
    var $a = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;
    var $b = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
    var $c = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0;
    var $d = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 0;
    var $e = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 0;
    var $f = arguments.length > 5 && arguments[5] !== undefined ? arguments[5] : 0;
    var $g = arguments.length > 6 && arguments[6] !== undefined ? arguments[6] : 0;
    var $h = arguments.length > 7 && arguments[7] !== undefined ? arguments[7] : 0;
    var $i = arguments.length > 8 && arguments[8] !== undefined ? arguments[8] : 0;
    var $j = arguments.length > 9 && arguments[9] !== undefined ? arguments[9] : 0;
    var $k = arguments.length > 10 && arguments[10] !== undefined ? arguments[10] : 0;
    var $l = arguments.length > 11 && arguments[11] !== undefined ? arguments[11] : 0;
    var $m = arguments.length > 12 && arguments[12] !== undefined ? arguments[12] : 0;
    var $n = arguments.length > 13 && arguments[13] !== undefined ? arguments[13] : 0;
    var $o = arguments.length > 14 && arguments[14] !== undefined ? arguments[14] : 0;
    var $p = arguments.length > 15 && arguments[15] !== undefined ? arguments[15] : 0;


    var matrix = [$a, $b, $c, $d, $e, $f, $g, $h, $i, $j, $k, $l, $m, $n, $o, $p].map(toFloatString);

    return shader('\n\n        outColor = vec4(\n            ' + matrix[0] + ' * pixelColor.r + ' + matrix[1] + ' * pixelColor.g + ' + matrix[2] + ' * pixelColor.b + ' + matrix[3] + ' * pixelColor.a,\n            ' + matrix[4] + ' * pixelColor.r + ' + matrix[5] + ' * pixelColor.g + ' + matrix[6] + ' * pixelColor.b + ' + matrix[7] + ' * pixelColor.a,\n            ' + matrix[8] + ' * pixelColor.r + ' + matrix[9] + ' * pixelColor.g + ' + matrix[10] + ' * pixelColor.b + ' + matrix[11] + ' * pixelColor.a,\n            ' + matrix[12] + ' * pixelColor.r + ' + matrix[13] + ' * pixelColor.g + ' + matrix[14] + ' * pixelColor.b + ' + matrix[15] + ' * pixelColor.a\n        ); \n    ');
}

function brownie$1() {

    return matrix$3(0.5997023498159715, 0.34553243048391263, -0.2708298674538042, 0, -0.037703249837783157, 0.8609577587992641, 0.15059552388459913, 0, 0.24113635128153335, -0.07441037908422492, 0.44972182064877153, 0, 0, 0, 0, 1);
}

/*
 * @param {Number} amount 0..1
 */
function clip$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;

    var C = toFloatString(parseParamNumber$1(amount));

    return shader('\n        outColor = vec4(\n            (pixelColor.r > 1.0 - ' + C + ') ? 1.0 : 0.0,\n            (pixelColor.g > 1.0 - ' + C + ') ? 1.0 : 0.0,\n            (pixelColor.b > 1.0 - ' + C + ') ? 1.0 : 0.0,\n            pixelColor.a \n        );\n    ');
}

function chaos() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 10;

    var C = toFloatString(parseParamNumber$1(amount));

    return shader('\n        vec2 st = pixelColor.st;\n        st *= ' + C + ';\n        \n        vec2 ipos = floor(st);  // get the integer coords\n\n        vec3 color = vec3(random( ipos ));\n\n        outColor = vec4(color, pixelColor.a);\n    ');
}

/*
 * @param {Number} amount  0..1
 */
function contrast$2() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var C = toFloatString(parseParamNumber$1(amount));

    return shader('\n        outColor = pixelColor * ' + C + ';\n    ');
}

/*
 * @param {Number} amount  -1..1  ,  value < 0  is darken, value > 0 is brighten 
 */
function gamma$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var C = toFloatString(parseParamNumber$1(amount));

    return shader('\n        outColor = vec4(pow(pixelColor.r, ' + C + '), pow(pixelColor.g, ' + C + '), pow(pixelColor.b, ' + C + '), pixelColor.a );\n    ');
}

/**
 * F.gradient('red', 'blue', 'yellow', 'white', 10)
 * F.gradient('red, blue, yellow, white, 10')
 */
function gradient$2() {
    // 전체 매개변수 기준으로 파싱 
    // 색이 아닌 것 기준으로 scale 변수로 인식 

    var params = [].concat(Array.prototype.slice.call(arguments));

    if (params.length === 1 && typeof params[0] === 'string') {
        params = Color$1.convertMatchesArray(params[0]);
    }

    params = params.map(function (arg) {
        return arg;
    }).join(', ');

    var colors = Color$1.parseGradient(params);

    colors[0][1] = 0;
    colors[colors.length - 1][1] = 1;

    colors = colors.map(function (c) {
        var _Color$parse = Color$1.parse(c[0]),
            r = _Color$parse.r,
            g = _Color$parse.g,
            b = _Color$parse.b,
            a = _Color$parse.a;

        return [{ r: r, g: g, b: b, a: a }, c[1]];
    });

    var temp = [];

    for (var i = 0, len = colors.length; i < len - 1; i++) {
        var start = colors[i];
        var end = colors[i + 1];

        var startColor = colorToVec4(start[0]);
        var endColor = colorToVec4(end[0]);

        var startRate = toFloatString(start[1]);
        var endRate = toFloatString(end[1]);

        temp.push('\n            if (' + startRate + ' <= rate && rate < ' + endRate + ') {\n                outColor = mix(' + startColor + ', ' + endColor + ', (rate - ' + startRate + ')/(' + endRate + ' - ' + startRate + '));\n            }\n        ');
    }

    return shader('\n        float rate = (pixelColor.r * 0.2126 + pixelColor.g * 0.7152 + pixelColor.b * 0.0722); \n\n        ' + temp.join('\n') + '        \n    ');
}

/**
 * 
 * @param {Number} amount 0..1
 */
function grayscale$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var C = parseParamNumber$1(amount);

    if (C > 1) C = 1;

    return matrix$3(0.2126 + 0.7874 * (1 - C), 0.7152 - 0.7152 * (1 - C), 0.0722 - 0.0722 * (1 - C), 0, 0.2126 - 0.2126 * (1 - C), 0.7152 + 0.2848 * (1 - C), 0.0722 - 0.0722 * (1 - C), 0, 0.2126 - 0.2126 * (1 - C), 0.7152 - 0.7152 * (1 - C), 0.0722 + 0.9278 * (1 - C), 0, 0, 0, 0, 1);
}

//http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
/*
 * @param {Number} amount  0..1  ,  (real value 0..360)
 */
function hue$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var C = toFloatString(parseParamNumber$1(amount));

    return shader('\n        vec3 hsv = rgb2hsv(pixelColor.rgb);\n        hsv.x += ' + C + ';\n        outColor = vec4(hsv2rgb(hsv).rgb, pixelColor.a);\n    ');
}

function invert$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var C = toFloatString(parseParamNumber$1(amount));

    return shader('\n        outColor = vec4(\n            (1.0 - pixelColor.r) * ' + C + ',\n            (1.0 - pixelColor.g) * ' + C + ',\n            (1.0 - pixelColor.b) * ' + C + ',\n            pixelColor.a\n        );\n    ');
}

function kodachrome$1() {

    return matrix$3(1.1285582396593525, -0.3967382283601348, -0.03992559172921793, 0, -0.16404339962244616, 1.0835251566291304, -0.05498805115633132, 0, -0.16786010706155763, -0.5603416277695248, 1.6014850761964943, 0, 0, 0, 0, 1);
}

/**
 * 
 * @param {Number} amount 0..1
 */
function noise$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;


    var C = Math.abs(parseParamNumber$1(amount));
    var min = toFloatString(-C);
    var max = toFloatString(C);
    return shader('\n        float rnd = ' + min + ' + random( pixelColor.st ) * (' + max + ' - ' + min + ');\n\n        outColor = vec4(pixelColor.rgb + rnd, 1.0);\n    ');
}

/**
 * 
 * @param {Number} amount 0..1
 */
function opacity$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var C = toFloatString(parseParamNumber$1(amount));

    return shader('\n        outColor = vec4(pixelColor.rgb, pixelColor.a * ' + C + ');\n    ');
}

function polaroid$1() {

    return matrix$3(1.438, -0.062, -0.062, 0, -0.122, 1.378, -0.122, 0, -0.016, -0.016, 1.483, 0, 0, 0, 0, 1);
}

/*
 * @param {Number} amount  0..1 
 */
function saturation$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;

    var L = 1 - Math.abs(parseParamNumber$1(amount));

    return matrix$3(L, 0, 0, 0, 0, L, 0, 0, 0, 0, L, 0, 0, 0, 0, L);
}

/*
 * @param {Number} amount  0..100 
 */
function sepia$1() {
    var amount = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    var C = parseParamNumber$1(amount);
    if (C > 1) C = 1;

    return matrix$3(0.393 + 0.607 * (1 - C), 0.769 - 0.769 * (1 - C), 0.189 - 0.189 * (1 - C), 0, 0.349 - 0.349 * (1 - C), 0.686 + 0.314 * (1 - C), 0.168 - 0.168 * (1 - C), 0, 0.272 - 0.272 * (1 - C), 0.534 - 0.534 * (1 - C), 0.131 + 0.869 * (1 - C), 0, 0, 0, 0, 1);
}

function shade$1() {
    var redValue = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;
    var greenValue = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 1;
    var blueValue = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 1;

    var r = toFloatString(parseParamNumber$1(redValue) / 255);
    var g = toFloatString(parseParamNumber$1(greenValue) / 255);
    var b = toFloatString(parseParamNumber$1(blueValue) / 255);

    return shader('\n        outColor = vec4(\n            pixelColor.r * ' + r + ',\n            pixelColor.g * ' + g + ',\n            pixelColor.b * ' + b + ',\n            pixelColor.a\n        );\n    ');
}

function shift$1() {

    return matrix$3(1.438, -0.062, -0.062, 0, -0.122, 1.378, -0.122, 0, -0.016, -0.016, 1.483, 0, 0, 0, 0, 1);
}

function solarize$1(redValue, greenValue, blueValue) {
    var r = toFloatString(parseParamNumber$1(redValue));
    var g = toFloatString(parseParamNumber$1(greenValue));
    var b = toFloatString(parseParamNumber$1(blueValue));

    return shader('\n        outColor = vec4(\n            (pixelColor.r < ' + r + ') ? 1.0 - pixelColor.r: pixelColor.r,\n            (pixelColor.g < ' + g + ') ? 1.0 - pixelColor.g: pixelColor.g,\n            (pixelColor.b < ' + b + ') ? 1.0 - pixelColor.b: pixelColor.b,\n            pixelColor.a\n        );\n    ');
}

function technicolor$1() {

    return matrix$3(1.9125277891456083, -0.8545344976951645, -0.09155508482755585, 0, -0.3087833385928097, 1.7658908555458428, -0.10601743074722245, 0, -0.231103377548616, -0.7501899197440212, 1.847597816108189, 0, 0, 0, 0, 1);
}

function thresholdColor$1() {
    var scale = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 1;

    scale = toFloatString(parseParamNumber$1(scale));

    return shader('\n        float c = ( (pixelColor.r * 0.2126 + pixelColor.g * 0.7152 + pixelColor.b * 0.0722) ) >= ' + scale + ' ? 1.0 : 0.0;\n\n        outColor = vec4(c, c, c, pixelColor.a);\n    ');
}

/*
 * @param {Number} amount  0..100 
 */
function threshold$1() {
  var scale = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 200;
  var amount = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 100;

  return thresholdColor$1(scale, amount, false);
}

/**
 * 
 * @param {*} redTint  0..1
 * @param {*} greenTint 0..1
 * @param {*} blueTint 0..1
 */
function tint$1 () {
    var redTint = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;
    var greenTint = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
    var blueTint = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0;

    var r = parseParamNumber$1(redTint);
    var g = parseParamNumber$1(greenTint);
    var b = parseParamNumber$1(blueTint);

    return shader('\n        outColor = vec4(\n            pixelColor.r += (1 - pixelColor.r) * ' + r + ',\n            pixelColor.g += (1 - pixelColor.g) * ' + g + ',\n            pixelColor.b += (1 - pixelColor.b) * ' + b + ',\n            pixelColor.a\n        );\n    ');
}

var pixel$2 = {
    bitonal: bitonal$1,
    brightness: brightness$2,
    brownie: brownie$1,
    clip: clip$1,
    chaos: chaos,
    contrast: contrast$2,
    gamma: gamma$1,
    gradient: gradient$2,
    grayscale: grayscale$1,
    hue: hue$1,
    invert: invert$1,
    kodachrome: kodachrome$1,
    matrix: matrix$3,
    noise: noise$1,
    opacity: opacity$1,
    polaroid: polaroid$1,
    saturation: saturation$1,
    sepia: sepia$1,
    shade: shade$1,
    shift: shift$1,
    solarize: solarize$1,
    technicolor: technicolor$1,
    threshold: threshold$1,
    'threshold-color': thresholdColor$1,
    tint: tint$1
};

function kirsch$1() {
    return multi$3('kirsch-horizontal kirsch-vertical');
}

function sobel$1() {
    return multi$3('sobel-horizontal sobel-vertical');
}

function vintage$1() {
    return multi$3('brightness(0.15) saturation(-0.2) gamma(1.8)');
}

var multi$4 = {
    kirsch: kirsch$1,
    sobel: sobel$1,
    vintage: vintage$1
};

var GLFilter = _extends({}, matrix$2, pixel$2, multi$4);

var TEXTURE_INDEX = 0;

var GLCanvas = function () {
    function GLCanvas() {
        var opt = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : {
            width: '400px',
            height: '300px'
        };
        classCallCheck(this, GLCanvas);

        this.img = opt.img;
        this.width = parseFloat(this.img.width || opt.width || '400px');
        this.height = parseFloat(this.img.height || opt.height || '300px');
        this.init();
    }

    createClass(GLCanvas, [{
        key: 'resize',
        value: function resize() {
            this.canvas.width = this.width;
            this.canvas.height = this.height;
            this.canvas.style.width = this.width + 'px';
            this.canvas.style.height = this.height + 'px';

            this.viewport();
        }

        /* Canvas 비우기, 비울 때 색 지정하기  */

    }, {
        key: 'clear',
        value: function clear() {
            var r = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;
            var g = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
            var b = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0;
            var a = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 0;

            var gl = this.gl;

            gl.clearColor(r, g, b, a);
            gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        }

        /* viewport 설정, 기본적으로 canvas 의 크기로 고정  */

    }, {
        key: 'viewport',
        value: function viewport(x, y, width, height) {
            var gl = this.gl;

            gl.viewport(x || 0, y || 0, width || gl.canvas.width, height || gl.canvas.height);
        }

        // canvas 초기화 
        // gl context 구하기 

    }, {
        key: 'initCanvas',
        value: function initCanvas(vertexSource, fragmentSource) {
            this.canvas = document.createElement('canvas');

            this.gl = this.canvas.getContext('webgl2');

            if (!this.gl) {
                throw new Error("you need webgl2 support");
            }

            // program 생성 
            this.program = this.createProgram(vertexSource, fragmentSource);

            // this.clear()
            this.resize();

            // buffer 설정 
            this.initBuffer();
        }
    }, {
        key: 'draw',
        value: function draw() {
            var primitiveType = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 'TRIANGLES';
            var offset = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
            var count = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 6;

            var gl = this.gl;

            gl.drawArrays(gl[primitiveType], offset, count);
        }
    }, {
        key: 'triangles',
        value: function triangles() {
            var offset = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;
            var count = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 6;

            this.draw('TRIANGLES', offset, count);
        }
    }, {
        key: 'uniform2f',
        value: function uniform2f() {
            var _gl;

            for (var _len = arguments.length, args = Array(_len), _key = 0; _key < _len; _key++) {
                args[_key] = arguments[_key];
            }

            var key = args.shift();

            (_gl = this.gl).uniform2f.apply(_gl, [this.locations[key]].concat(args));
        }
    }, {
        key: 'uniform1f',
        value: function uniform1f() {
            var _gl2;

            for (var _len2 = arguments.length, args = Array(_len2), _key2 = 0; _key2 < _len2; _key2++) {
                args[_key2] = arguments[_key2];
            }

            var key = args.shift();

            (_gl2 = this.gl).uniform1f.apply(_gl2, [this.locations[key]].concat(args));
        }
    }, {
        key: 'uniform1fv',
        value: function uniform1fv() {
            var _gl3;

            for (var _len3 = arguments.length, args = Array(_len3), _key3 = 0; _key3 < _len3; _key3++) {
                args[_key3] = arguments[_key3];
            }

            var key = args.shift();

            (_gl3 = this.gl).uniform1fv.apply(_gl3, [this.locations[key]].concat(args));
        }
    }, {
        key: 'uniform1i',
        value: function uniform1i() {
            var _gl4;

            for (var _len4 = arguments.length, args = Array(_len4), _key4 = 0; _key4 < _len4; _key4++) {
                args[_key4] = arguments[_key4];
            }

            var key = args.shift();

            (_gl4 = this.gl).uniform1i.apply(_gl4, [this.locations[key]].concat(args));
        }
    }, {
        key: 'useProgram',
        value: function useProgram() {
            var gl = this.gl;

            gl.useProgram(this.program);
        }
    }, {
        key: 'bindBuffer',
        value: function bindBuffer(key, data) {
            var drawType = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 'STATIC_DRAW';

            var gl = this.gl;

            if (!this.buffers[key]) {
                this.buffers[key] = gl.createBuffer();
            }

            gl.bindBuffer(gl.ARRAY_BUFFER, this.buffers[key]);

            if (data) {
                gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(data), gl[drawType]);
            }
        }
    }, {
        key: 'enable',
        value: function enable(key) {
            var gl = this.gl;

            // array attribute 를 활성화 시킴 
            gl.enableVertexAttribArray(this.locations[key]);
        }
    }, {
        key: 'location',
        value: function location(key) {
            var type = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : "attribute";

            if (type === 'attribute') {
                this.locations[key] = this.gl.getAttribLocation(this.program, key);
            } else if (type === 'uniform') {
                this.locations[key] = this.gl.getUniformLocation(this.program, key);
            }
        }
    }, {
        key: 'a',
        value: function a(key) {
            return this.location(key);
        }
    }, {
        key: 'u',
        value: function u(key) {
            return this.location(key, "uniform");
        }
    }, {
        key: 'pointer',
        value: function pointer(key) {
            var type = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 'FLOAT';
            var size = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 2;
            var normalize = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : false;
            var stride = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 0;
            var offset = arguments.length > 5 && arguments[5] !== undefined ? arguments[5] : 0;

            var gl = this.gl;

            gl.vertexAttribPointer(this.locations[key], size, gl[type], normalize, stride, offset);
        }
    }, {
        key: 'bufferData',
        value: function bufferData() {
            var data = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : [];

            var gl = this.gl;
            gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(data), gl.STATIC_DRAW);
        }
    }, {
        key: 'isPowerOf2',
        value: function isPowerOf2(value) {
            return (value & value - 1) == 0;
        }
    }, {
        key: 'bindTexture',
        value: function bindTexture(key) {
            var img = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : undefined;
            var mipLevel = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0;
            var internalFormat = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 'RGBA';
            var srcFormat = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 'RGBA';
            var srcType = arguments.length > 5 && arguments[5] !== undefined ? arguments[5] : 'UNSIGNED_BYTE';

            var gl = this.gl;

            if (arguments.length == 1) {
                gl.bindTexture(gl.TEXTURE_2D, this.textures[key]);
                return;
            }

            if (!this.textures[key]) {
                this.textures[key] = gl.createTexture();
            }

            this.textureIndex[key] = TEXTURE_INDEX++;
            // this.activeTexture(key)
            gl.bindTexture(gl.TEXTURE_2D, this.textures[key]);

            this.setTextureParameter();

            gl.texImage2D(gl.TEXTURE_2D, mipLevel, gl[internalFormat], gl[srcFormat], gl[srcType], img.newImage || img);
        }
    }, {
        key: 'bindColorTexture',
        value: function bindColorTexture(key, data) {
            var width = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 256;
            var height = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 1;
            var mipLevel = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 0;
            var internalFormat = arguments.length > 5 && arguments[5] !== undefined ? arguments[5] : 'RGBA';
            var srcFormat = arguments.length > 6 && arguments[6] !== undefined ? arguments[6] : 'RGBA';
            var srcType = arguments.length > 7 && arguments[7] !== undefined ? arguments[7] : 'UNSIGNED_BYTE';

            var gl = this.gl;

            if (!this.textures[key]) {
                this.textures[key] = gl.createTexture();
            }

            this.textureIndex[key] = TEXTURE_INDEX++;
            gl.bindTexture(gl.TEXTURE_2D, this.textures[key]);

            this.setTextureParameter();

            gl.texImage2D(gl.TEXTURE_2D, mipLevel, gl[internalFormat], width, height, 0, gl[srcFormat], gl[srcType], new Uint8Array(data));
        }
    }, {
        key: 'bindEmptyTexture',
        value: function bindEmptyTexture(key, width, height) {
            var mipLevel = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 0;
            var internalFormat = arguments.length > 4 && arguments[4] !== undefined ? arguments[4] : 'RGBA';
            var srcFormat = arguments.length > 5 && arguments[5] !== undefined ? arguments[5] : 'RGBA';
            var srcType = arguments.length > 6 && arguments[6] !== undefined ? arguments[6] : 'UNSIGNED_BYTE';

            var gl = this.gl;

            if (!this.textures[key]) {
                this.textures[key] = gl.createTexture();
            }

            this.textureIndex[key] = TEXTURE_INDEX++;
            gl.bindTexture(gl.TEXTURE_2D, this.textures[key]);

            this.setTextureParameter();

            var border = 0;
            var data = null;

            gl.texImage2D(gl.TEXTURE_2D, mipLevel, gl[internalFormat], width, height, border, gl[srcFormat], gl[srcType], data);
        }
    }, {
        key: 'setTextureParameter',
        value: function setTextureParameter() {
            var gl = this.gl;

            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
            gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
        }
    }, {
        key: 'bindFrameBufferWithTexture',
        value: function bindFrameBufferWithTexture(key, textureKey, width, height) {
            this.bindEmptyTexture(textureKey, width, height);
            this.bindFrameBuffer(key, textureKey);
        }
    }, {
        key: 'enumToString',
        value: function enumToString(value) {
            var gl = this.gl;

            if (value === 0) {
                return "NONE";
            }
            for (var key in gl) {
                if (gl[key] === value) {
                    return key;
                }
            }
            return "0x" + value.toString(16);
        }
    }, {
        key: 'bindFrameBuffer',
        value: function bindFrameBuffer(key) {
            var textureKey = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : null;

            var gl = this.gl;

            if (arguments.length === 1) {
                gl.bindFramebuffer(gl.FRAMEBUFFER, key == null ? null : this.framebuffers[key]);
                return;
            }

            if (!this.framebuffers[key]) {
                // 프레임버퍼 생성하기 
                this.framebuffers[key] = gl.createFramebuffer();
            }

            gl.bindFramebuffer(gl.FRAMEBUFFER, this.framebuffers[key]);

            // framebuffer 에 texture2d 연결 
            var mipLevel = 0;
            var attachmentPoint = gl.COLOR_ATTACHMENT0; // framebuffer 를 attachmentPoint 에 연결한다. 
            // framebuffer 는 데이타를 가지고 있지 않고 연결 고리만 가지고 있다. 
            gl.framebufferTexture2D(gl.FRAMEBUFFER, attachmentPoint, gl.TEXTURE_2D, this.textures[textureKey], mipLevel);

            // framebuffer 상태 체크 하기 
            // framebuffer 를 더 이상 할당 못할 수도 있음. 
            var status = gl.checkFramebufferStatus(gl.FRAMEBUFFER);

            // console.log(this.enumToString(attachmentPoint), this.enumToString(status), key, this.textures[textureKey]);

            if (status !== gl.FRAMEBUFFER_COMPLETE) {
                return;
            }
        }
    }, {
        key: 'bindVA',
        value: function bindVA() {
            var gl = this.gl;

            if (!this.vao) {
                this.vao = gl.createVertexArray();
            }

            gl.bindVertexArray(this.vao);
        }
    }, {
        key: 'bindAttr',
        value: function bindAttr(key, data) {
            var drawType = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 'STATIC_DRAW';
            var size = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : 2;

            // 버퍼를 만들고 데이타를 연결한다. 
            this.bindBuffer(key, data, drawType);

            //array 변수를 사용할 수 있도록 활성화 시킨다. 
            this.enable(key);

            // 포인터를 지정한다. 
            // array 변수가 어떻게 iteration 될지 지정한다. size 는 한번에 연산될 요소 개수 
            // size 가 2 라고 했을 때 2개씩 하나의 iteration 에 들어간다. 
            // 즉, (x, y) 가 한번에 들어감 
            this.pointer(key, 'FLOAT', size);
        }

        /* 
            shader 에서 사용하는 Attribute, Uniform 변수 설정 
            변수 설정을 간소화 할 필요도 있을 듯 하다. 
        */

    }, {
        key: 'initBuffer',
        value: function initBuffer() {
            var _canvas = this.canvas,
                width = _canvas.width,
                height = _canvas.height;

            // console.log(width, height)

            // 선언된 변수 location 지정 하기 
            // location 을 지정해야 GLSL 에서 해당 변수와 연결할 수 있다. 언제? 

            this.a("a_position");
            this.a("a_texCoord");
            this.u("u_resolution");
            this.u("u_image");
            this.u("u_flipY");

            this.u("u_kernelSelect");
            this.u("u_filterIndex");

            this.u("u_kernel9[0]");
            this.u("u_kernel9Weight");
            this.u("u_kernel25[0]");
            this.u("u_kernel25Weight");
            this.u("u_kernel49[0]");
            this.u("u_kernel49Weight");
            this.u("u_kernel81[0]");
            this.u("u_kernel81Weight");

            this.bindVA();

            // 단순 변수를 초기화 하고 
            this.bindAttr("a_position", [0, 0, width, 0, 0, height, 0, height, width, 0, width, height], 'STATIC_DRAW', 2 /* components for iteration */);

            // 변수에 데이타를 연결할다. 
            this.bindAttr("a_texCoord", [0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0], 'STATIC_DRAW', 2 /* components for iteration */);

            // texture 는 img 로 할 수도 있고 
            this.bindTexture("u_image", this.img);

            // 비어있는 texture 도 만들 수 있다. 
            // 객체로 제어할까? 
            // texture 를 framebuffer 로 바로 대응시킨다. 
            // 이후 framebuffer 가 변경되면 img_texture 가 바뀐다. 
            this.bindFrameBufferWithTexture("frame_buffer_0", "img_texture_0", width, height);
            this.bindFrameBufferWithTexture("frame_buffer_1", "img_texture_1", width, height);
        }
    }, {
        key: 'activeTexture',
        value: function activeTexture() {
            var index = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : 0;

            var gl = this.gl;

            gl.activeTexture(gl.TEXTURE0 + index);
        }
    }, {
        key: 'drawFilter',
        value: function drawFilter() {
            var _this = this;

            var gl = this.gl;

            this.resize();
            this.clear();

            this.useProgram();

            this.bindVA();

            this.activeTexture(0);
            this.bindTexture('u_image');

            this.uniform1i("u_image", 0);
            this.uniform1f("u_flipY", 1);

            var _gl$canvas = gl.canvas,
                width = _gl$canvas.width,
                height = _gl$canvas.height;


            this.eachFilter(function (f, index) {

                _this.bindFrameBuffer('frame_buffer_' + index % 2);
                _this.uniform2f("u_resolution", width, height);
                _this.viewport(0, 0, width, height);

                _this.effectFilter(f);

                // 다음 드로잉을 위해 방금 렌더링 한 텍스처를 사용합니다.
                _this.bindTexture('img_texture_' + index % 2);
            });

            this.uniform1f("u_flipY", -1);
            this.bindFrameBuffer(null);
            this.uniform2f("u_resolution", width, height);
            this.viewport(0, 0, width, height);

            // clear 가 있는 이유는? 
            this.clear();

            this.effectFilter("normal");
        }
    }, {
        key: 'effectFilter',
        value: function effectFilter(filterFunction) {

            if (typeof filterFunction == 'string') {
                filterFunction = (GLFilter[filterFunction] || GLFilter.normal).call(GLFilter);
            }

            if (filterFunction.type == 'convolution') {
                this.uniform1f("u_kernelSelect", filterFunction.length);
                this.uniform1f("u_filterIndex", -1.0);
                this.uniform1fv('u_kernel' + filterFunction.length + '[0]', filterFunction.content);
                this.uniform1f('u_kernel' + filterFunction.length + 'Weight', this.computeKernelWeight(filterFunction.content));
            } else {

                this.uniform1f("u_kernelSelect", -1.0);
                this.uniform1f("u_filterIndex", filterFunction.index);
            }

            this.triangles(0 /* 시작 지점 */, 6 /* 좌표(vertex, 꼭지점) 개수 */); // 총 6개를 도는데 , triangles 니깐 3개씩 묶어서 2번 돈다. 
        }
    }, {
        key: 'computeKernelWeight',
        value: function computeKernelWeight(kernel) {
            var weight = kernel.reduce(function (prev, curr) {
                return prev + curr;
            });
            return weight <= 0 ? 1 : weight;
        }
    }, {
        key: 'createProgram',
        value: function createProgram(vertexSource, fragmentSource) {

            var gl = this.gl;

            var program = gl.createProgram();

            this.vertexShader = this.createVertexShader(vertexSource);
            this.fragmentShader = this.createFragmentShader(fragmentSource);

            // console.log(fragmentSource)      


            gl.attachShader(program, this.vertexShader);
            gl.attachShader(program, this.fragmentShader);

            gl.linkProgram(program);

            var success = gl.getProgramParameter(program, gl.LINK_STATUS);
            if (success) {

                return program;
            }

            console.error(gl.getProgramInfoLog(program));
            gl.deleteProgram(program);
        }
    }, {
        key: 'createShader',
        value: function createShader(type, source) {
            var gl = this.gl;

            var shader$$1 = gl.createShader(type);
            gl.shaderSource(shader$$1, source);
            gl.compileShader(shader$$1);

            var success = gl.getShaderParameter(shader$$1, gl.COMPILE_STATUS);

            if (success) {
                return shader$$1;
            }

            console.error(gl.getShaderInfoLog(shader$$1));
            gl.deleteShader(shader$$1);
        }
    }, {
        key: 'createVertexShader',
        value: function createVertexShader(vertexSource) {
            var gl = this.gl;

            return this.createShader(gl.VERTEX_SHADER, vertexSource);
        }
    }, {
        key: 'createFragmentShader',
        value: function createFragmentShader(fragmentSource) {
            var gl = this.gl;

            return this.createShader(gl.FRAGMENT_SHADER, fragmentSource);
        }
    }, {
        key: 'eachFilter',
        value: function eachFilter(callback) {
            this.filterList.forEach(callback);
        }
    }, {
        key: 'init',
        value: function init() {
            this.locations = {};
            this.buffers = {};
            this.framebuffers = {};
            this.textures = {};
            this.textureIndex = {};
            this.hasTexParameter = {};
        }
    }, {
        key: 'destroy',
        value: function destroy() {
            var gl = this.gl;

            this.init();

            gl.deleteProgram(this.program);
        }
    }, {
        key: 'filter',
        value: function filter(filterList, doneCallback) {

            this.filterList = filterList;

            this.initCanvas(makeVertexShaderSource(), makeFragmentShaderSource(this.filterList));

            this.drawFilter();

            if (typeof doneCallback == 'function') {

                doneCallback(this);
            }
        }
    }]);
    return GLCanvas;
}();

var GL$1 = {
    GLCanvas: GLCanvas
};

var functions = {
    filter: filter
};






function makeFilterFunction(filterObj) {
    var filterName = filterObj.arr[0];
    var f = GLFilter[filterName];

    var arr = filterObj.arr;
    arr.shift();

    var result = f.apply(this, arr);

    return result;
}

/**
 * 겹쳐져 있는 Filter 함수를 1차원으로 나열한다. 
 * ex)  ['sobel'] => ['sobel-horizontal', 'sobel-vertial'] 
 * 
 * @param {String|Array} filterString 
 */
function flatFilter(filterString) {

    var filter_list = [];

    if (typeof filterString == 'string') {
        filter_list = matches$1(filterString);
    } else if (Array.isArray(filterString)) {
        filter_list = filterString;
    }

    var allFilter = [];

    filter_list.forEach(function (filterObj) {
        var filterName = filterObj.arr[0];

        if (GLFilter[filterName]) {
            var f = makeFilterFunction(filterObj);

            if (f.type == 'convolution' || f.type == 'shader') {
                allFilter.push(f);
            } else {
                f.forEach(function (subFilter) {
                    allFilter = allFilter.concat(flatFilter(subFilter));
                });
            }
        }
    });

    // console.log(filter_list, allFilter)

    return allFilter;
}

function filter(img, filterString, callback, opt) {

    var canvas = new GL$1.GLCanvas({
        width: opt.width || img.width,
        height: opt.height || img.height,
        img: img
    });

    canvas.filter(flatFilter(filterString), function done() {
        if (typeof callback == 'function') {
            callback(canvas);
        }
    });
}

var GL = _extends({}, GL$1, functions);

function palette(colors) {
    var k = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 6;
    var exportFormat = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 'hex';


    if (colors.length > k) {
        colors = kmeans(colors, k);
    }

    return colors.map(function (c) {
        return format(c, exportFormat);
    });
}

function ImageToRGB(url) {
    var callbackOrOption = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
    var callback = arguments[2];


    if (!callback) {
        var img = new ImageLoader(url);
        img.loadImage(function () {
            if (typeof callbackOrOption == 'function') {
                callbackOrOption(img.toRGB());
            }
        });
    } else if (callback) {
        var img = new ImageLoader(url, callbackOrOption);
        img.loadImage(function () {
            if (typeof callback == 'function') {
                callback(img.toRGB());
            }
        });
    }
}

function ImageToCanvas(url, filter, callback) {
    var opt = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : { frameTimer: 'full' };

    ImageToURL(url, filter, callback, Object.assign({
        returnTo: 'canvas'
    }, opt));
}

function ImageToURL(url, filter, callback) {
    var opt = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : { frameTimer: 'full' };

    var img = new ImageLoader(url);
    img.loadImage(function () {
        img.toArray(filter, function (datauri) {
            if (typeof callback == 'function') {
                callback(datauri);
            }
        }, opt);
    });
}

function GLToCanvas(url, filter, callback) {
    var opt = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : {};

    var img = new ImageLoader(url);
    img.load(function () {
        GL.filter(img.newImage, filter, function done(datauri) {
            if (typeof callback == 'function') {
                callback(datauri);
            }
        }, opt);
    });
}

function histogram(url, callback) {
    var opt = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};

    var img = new ImageLoader(url);
    img.loadImage(function () {
        if (typeof callback == 'function') {
            callback(img.toHistogram(opt));
        }
    });
}

function histogramToPoints(points) {
    var tension = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0.2;


    var controlPoints = [];
    for (var i = 0; i < points.length; i++) {
        var p = points[i];
        if (i == 0) {
            controlPoints[i] = [];
            continue;
        }

        if (i == points.length - 1) {
            controlPoints[i] = [];
            continue;
        }

        var prevPoint = points[i - 1];
        var nextPoint = points[i + 1];

        // 기울기 
        var M = (nextPoint[1] - prevPoint[1]) / (nextPoint[0] - prevPoint[0]);

        var newControlPoint = [prevPoint[0] + (nextPoint[0] - prevPoint[0]) * tension, prevPoint[1] + (nextPoint[1] - prevPoint[1]) * tension];

        var controlPoint = [[].concat(toConsumableArray(prevPoint)), /* start */
        [].concat(newControlPoint) /* end */
        ];

        var P = Math.sqrt(Math.pow(p[0] - prevPoint[0], 2) + Math.pow(p[1] - prevPoint[1], 2));
        var N = Math.sqrt(Math.pow(nextPoint[0] - p[0], 2) + Math.pow(nextPoint[1] - p[1], 2));

        var rate = P / N;

        var dx = controlPoint[0][0] + (controlPoint[1][0] - controlPoint[0][0]) * rate;
        var dy = controlPoint[0][1] + (controlPoint[1][1] - controlPoint[0][1]) * rate;

        controlPoint[0][0] += p[0] - dx;
        controlPoint[0][1] += p[1] - dy;
        controlPoint[1][0] += p[0] - dx;
        controlPoint[1][1] += p[1] - dy;

        controlPoints[i] = controlPoint;
    }

    return controlPoints;
}

function ImageToHistogram(url, callback) {
    var opt = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : { width: 200, height: 100 };


    var img = new ImageLoader(url);
    img.loadImage(function () {
        Canvas.createHistogram(opt.width || 200, opt.height || 100, img.toHistogram(opt), function (canvas) {
            if (typeof callback == 'function') callback(canvas.toDataURL('image/png'));
        }, opt);
    });
}

var image = {
    palette: palette,
    ImageToCanvas: ImageToCanvas,
    ImageToHistogram: ImageToHistogram,
    ImageToRGB: ImageToRGB,
    ImageToURL: ImageToURL,
    GLToCanvas: GLToCanvas,
    histogram: histogram,
    histogramToPoints: histogramToPoints
};

var Color$1 = _extends({}, formatter, math, mixin, parser, fromYCrCb, fromRGB, fromCMYK, fromHSV, fromHSL, fromLAB, image);

var hue_color = [{ rgb: '#ff0000', start: .0 }, { rgb: '#ffff00', start: .17 }, { rgb: '#00ff00', start: .33 }, { rgb: '#00ffff', start: .50 }, { rgb: '#0000ff', start: .67 }, { rgb: '#ff00ff', start: .83 }, { rgb: '#ff0000', start: 1 }];

function checkHueColor(p) {
    var startColor, endColor;

    for (var i = 0; i < hue_color.length; i++) {
        if (hue_color[i].start >= p) {
            startColor = hue_color[i - 1];
            endColor = hue_color[i];
            break;
        }
    }

    if (startColor && endColor) {
        return Color$1.interpolateRGB(startColor, endColor, (p - startColor.start) / (endColor.start - startColor.start));
    }

    return hue_color[0].rgb;
}

function initHueColors() {
    for (var i = 0, len = hue_color.length; i < len; i++) {
        var hue = hue_color[i];

        var obj = Color$1.parse(hue.rgb);

        hue.r = obj.r;
        hue.g = obj.g;
        hue.b = obj.b;
    }
}

initHueColors();

var HueColor = {
    colors: hue_color,
    checkHueColor: checkHueColor
};

// TODO: worker run 
var ImageFilter = _extends({}, FilterList, functions$1);

var Util = {
    Color: Color$1,
    HueColor: HueColor,
    ColorNames: ColorNames,
    ImageFilter: ImageFilter,
    GL: GL,
    Canvas: Canvas,
    ImageLoader: ImageLoader
};

var color = Color$1.color;

var counter = 0;
var cached = [];

var Dom = function () {
    function Dom(tag, className, attr) {
        classCallCheck(this, Dom);


        if (typeof tag != 'string') {
            this.el = tag;
        } else {

            var el = document.createElement(tag);
            this.uniqId = counter++;

            if (className) {
                el.className = className;
            }

            attr = attr || {};

            for (var k in attr) {
                el.setAttribute(k, attr[k]);
            }

            this.el = el;
        }
    }

    createClass(Dom, [{
        key: 'attr',
        value: function attr(key, value) {
            if (arguments.length == 1) {
                return this.el.getAttribute(key);
            }

            this.el.setAttribute(key, value);

            return this;
        }
    }, {
        key: 'closest',
        value: function closest(cls) {

            var temp = this;
            var checkCls = false;

            while (!(checkCls = temp.hasClass(cls))) {
                if (temp.el.parentNode) {
                    temp = new Dom(temp.el.parentNode);
                } else {
                    return null;
                }
            }

            if (checkCls) {
                return temp;
            }

            return null;
        }
    }, {
        key: 'checked',
        value: function checked() {
            return this.el.checked;
        }
    }, {
        key: 'removeClass',
        value: function removeClass(cls) {
            this.el.className = (' ' + this.el.className + ' ').replace(' ' + cls + ' ', ' ').trim();

            return this;
        }
    }, {
        key: 'hasClass',
        value: function hasClass(cls) {
            if (!this.el.className) {
                return false;
            } else {
                var newClass = ' ' + this.el.className + ' ';
                return newClass.indexOf(' ' + cls + ' ') > -1;
            }
        }
    }, {
        key: 'addClass',
        value: function addClass(cls) {
            if (!this.hasClass(cls)) {
                this.el.className = this.el.className + ' ' + cls;
            }

            return this;
        }
    }, {
        key: 'toggleClass',
        value: function toggleClass(cls) {
            if (this.hasClass(cls)) {
                this.removeClass(cls);
            } else {
                this.addClass(cls);
            }
        }
    }, {
        key: 'html',
        value: function html(_html) {
            try {
                if (typeof _html == 'string') {
                    this.el.innerHTML = _html;
                } else {
                    this.empty().append(_html);
                }
            } catch (e) {
                console.log(_html);
            }

            return this;
        }
    }, {
        key: 'find',
        value: function find(selector) {
            return this.el.querySelector(selector);
        }
    }, {
        key: '$',
        value: function $(selector) {
            return new Dom(this.find(selector));
        }
    }, {
        key: 'findAll',
        value: function findAll(selector) {
            return this.el.querySelectorAll(selector);
        }
    }, {
        key: '$$',
        value: function $$(selector) {
            return [].concat(toConsumableArray(this.findAll(selector))).map(function (el) {
                return new Dom(el);
            });
        }
    }, {
        key: 'empty',
        value: function empty() {
            return this.html('');
        }
    }, {
        key: 'append',
        value: function append(el) {

            if (typeof el == 'string') {
                this.el.appendChild(document.createTextNode(el));
            } else {
                this.el.appendChild(el.el || el);
            }

            return this;
        }
    }, {
        key: 'appendTo',
        value: function appendTo(target) {
            var t = target.el ? target.el : target;

            t.appendChild(this.el);

            return this;
        }
    }, {
        key: 'remove',
        value: function remove() {
            if (this.el.parentNode) {
                this.el.parentNode.removeChild(this.el);
            }

            return this;
        }
    }, {
        key: 'text',
        value: function text() {
            return this.el.textContent;
        }
    }, {
        key: 'css',
        value: function css(key, value) {
            var _this = this;

            if (arguments.length == 2) {
                this.el.style[key] = value;
            } else if (arguments.length == 1) {

                if (typeof key == 'string') {
                    return getComputedStyle(this.el)[key];
                } else {
                    var keys = key || {};
                    Object.keys(keys).forEach(function (k) {
                        _this.el.style[k] = keys[k];
                    });
                }
            }

            return this;
        }
    }, {
        key: 'cssFloat',
        value: function cssFloat(key) {
            return parseFloat(this.css(key));
        }
    }, {
        key: 'cssInt',
        value: function cssInt(key) {
            return parseInt(this.css(key));
        }
    }, {
        key: 'offset',
        value: function offset() {
            var rect = this.el.getBoundingClientRect();

            return {
                top: rect.top + Dom.getScrollTop(),
                left: rect.left + Dom.getScrollLeft()
            };
        }
    }, {
        key: 'rect',
        value: function rect() {
            return this.el.getBoundingClientRect();
        }
    }, {
        key: 'position',
        value: function position() {

            if (this.el.style.top) {
                return {
                    top: parseFloat(this.css('top')),
                    left: parseFloat(this.css('left'))
                };
            } else {
                return this.el.getBoundingClientRect();
            }
        }
    }, {
        key: 'size',
        value: function size() {
            return [this.width(), this.height()];
        }
    }, {
        key: 'width',
        value: function width() {
            return this.el.offsetWidth || this.el.getBoundingClientRect().width;
        }
    }, {
        key: 'contentWidth',
        value: function contentWidth() {
            return this.width() - this.cssFloat('padding-left') - this.cssFloat('padding-right');
        }
    }, {
        key: 'height',
        value: function height() {
            return this.el.offsetHeight || this.el.getBoundingClientRect().height;
        }
    }, {
        key: 'contentHeight',
        value: function contentHeight() {
            return this.height() - this.cssFloat('padding-top') - this.cssFloat('padding-bottom');
        }
    }, {
        key: 'dataKey',
        value: function dataKey(key) {
            return this.uniqId + '.' + key;
        }
    }, {
        key: 'data',
        value: function data(key, value) {
            if (arguments.length == 2) {
                cached[this.dataKey(key)] = value;
            } else if (arguments.length == 1) {
                return cached[this.dataKey(key)];
            } else {
                var keys = Object.keys(cached);

                var uniqId = this.uniqId + ".";
                return keys.filter(function (key) {
                    if (key.indexOf(uniqId) == 0) {
                        return true;
                    }

                    return false;
                }).map(function (value) {
                    return cached[value];
                });
            }

            return this;
        }
    }, {
        key: 'val',
        value: function val(value) {
            if (arguments.length == 0) {
                return this.el.value;
            } else if (arguments.length == 1) {
                this.el.value = value;
            }

            return this;
        }
    }, {
        key: 'int',
        value: function int() {
            return parseInt(this.val(), 10);
        }
    }, {
        key: 'float',
        value: function float() {
            return parseFloat(this.val());
        }
    }, {
        key: 'show',
        value: function show() {
            return this.css('display', 'block');
        }
    }, {
        key: 'hide',
        value: function hide() {
            return this.css('display', 'none');
        }
    }, {
        key: 'toggle',
        value: function toggle() {
            if (this.css('display') == 'none') {
                return this.show();
            } else {
                return this.hide();
            }
        }
    }, {
        key: 'scrollTop',
        value: function scrollTop() {
            if (this.el === document.body) {
                return Dom.getScrollTop();
            }

            return this.el.scrollTop;
        }
    }, {
        key: 'scrollLeft',
        value: function scrollLeft() {
            if (this.el === document.body) {
                return Dom.getScrollLeft();
            }

            return this.el.scrollLeft;
        }
    }, {
        key: 'on',
        value: function on(eventName, callback, opt1, opt2) {
            this.el.addEventListener(eventName, callback, opt1, opt2);

            return this;
        }
    }, {
        key: 'off',
        value: function off(eventName, callback) {
            this.el.removeEventListener(eventName, callback);

            return this;
        }
    }, {
        key: 'getElement',
        value: function getElement() {
            return this.el;
        }
    }, {
        key: 'createChild',
        value: function createChild(tag) {
            var className = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : '';
            var attrs = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : {};
            var css = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : {};

            var $element = new Dom(tag, className, attrs);
            $element.css(css);

            this.append($element);

            return $element;
        }
    }, {
        key: 'firstChild',
        value: function firstChild() {
            return new Dom(this.el.firstElementChild);
        }
    }, {
        key: 'replace',
        value: function replace(oldElement, newElement) {
            this.el.replaceChild(newElement, oldElement);

            return this;
        }
    }], [{
        key: 'getScrollTop',
        value: function getScrollTop() {
            return Math.max(window.pageYOffset, document.documentElement.scrollTop, document.body.scrollTop);
        }
    }, {
        key: 'getScrollLeft',
        value: function getScrollLeft() {
            return Math.max(window.pageXOffset, document.documentElement.scrollLeft, document.body.scrollLeft);
        }
    }]);
    return Dom;
}();

var BaseModule = function () {
    function BaseModule($store) {
        classCallCheck(this, BaseModule);

        this.$store = $store;
        this.initialize();
    }

    createClass(BaseModule, [{
        key: 'initialize',
        value: function initialize() {
            var _this = this;

            this.filterProps().forEach(function (key) {
                _this.$store.action(key, _this);
            });
        }
    }, {
        key: 'filterProps',
        value: function filterProps() {
            var pattern = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : '/';

            return Object.getOwnPropertyNames(this.__proto__).filter(function (key) {
                return key.startsWith(pattern);
            });
        }
    }]);
    return BaseModule;
}();

var ColorSetsList = function (_BaseModule) {
    inherits(ColorSetsList, _BaseModule);

    function ColorSetsList() {
        classCallCheck(this, ColorSetsList);
        return possibleConstructorReturn(this, (ColorSetsList.__proto__ || Object.getPrototypeOf(ColorSetsList)).apply(this, arguments));
    }

    createClass(ColorSetsList, [{
        key: 'initialize',
        value: function initialize() {
            get(ColorSetsList.prototype.__proto__ || Object.getPrototypeOf(ColorSetsList.prototype), 'initialize', this).call(this);

            // set property
            this.$store.colorSetsList = [{ name: "Material",
                colors: ['#F44336', '#E91E63', '#9C27B0', '#673AB7', '#3F51B5', '#2196F3', '#03A9F4', '#00BCD4', '#009688', '#4CAF50', '#8BC34A', '#CDDC39', '#FFEB3B', '#FFC107', '#FF9800', '#FF5722', '#795548', '#9E9E9E', '#607D8B']
            }, { name: "Custom", "edit": true, "colors": [] }, { name: "Color Scale", "scale": ['red', 'yellow', 'black'], count: 5 }];
            this.$store.currentColorSets = {};
        }
    }, {
        key: '/list',
        value: function list($store) {
            return Array.isArray($store.userList) && $store.userList.length ? $store.userList : $store.colorSetsList;
        }
    }, {
        key: '/setUserPalette',
        value: function setUserPalette($store, list) {
            $store.userList = list;

            $store.dispatch('/resetUserPalette');
            $store.dispatch('/setCurrentColorSets');
        }
    }, {
        key: '/resetUserPalette',
        value: function resetUserPalette($store) {
            if ($store.userList && $store.userList.length) {
                $store.userList = $store.userList.map(function (element, index) {

                    if (typeof element.colors == 'function') {
                        var makeCallback = element.colors;

                        element.colors = makeCallback($store);
                        element._colors = makeCallback;
                    }

                    return Object.assign({
                        name: 'color-' + index,
                        colors: []
                    }, element);
                });

                $store.emit('changeUserList');
            }
        }
    }, {
        key: '/setCurrentColorSets',
        value: function setCurrentColorSets($store, nameOrIndex) {

            var _list = $store.dispatch('/list');

            if (typeof nameOrIndex == 'undefined') {
                $store.currentColorSets = _list[0];
            } else if (typeof nameOrIndex == 'number') {
                $store.currentColorSets = _list[nameOrIndex];
            } else {
                $store.currentColorSets = _list.filter(function (obj) {
                    return obj.name == nameOrIndex;
                })[0];
            }

            $store.emit('changeCurrentColorSets');
        }
    }, {
        key: '/getCurrentColorSets',
        value: function getCurrentColorSets($store) {
            return $store.currentColorSets;
        }
    }, {
        key: '/addCurrentColor',
        value: function addCurrentColor($store, color) {
            if (Array.isArray($store.currentColorSets.colors)) {
                $store.currentColorSets.colors.push(color);
                $store.emit('changeCurrentColorSets');
            }
        }
    }, {
        key: '/setCurrentColorAll',
        value: function setCurrentColorAll($store) {
            var colors = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : [];

            $store.currentColorSets.colors = colors;
            $store.emit('changeCurrentColorSets');
        }
    }, {
        key: '/removeCurrentColor',
        value: function removeCurrentColor($store, index) {
            if ($store.currentColorSets.colors[index]) {
                $store.currentColorSets.colors.splice(index, 1);
                $store.emit('changeCurrentColorSets');
            }
        }
    }, {
        key: '/removeCurrentColorToTheRight',
        value: function removeCurrentColorToTheRight($store, index) {
            if ($store.currentColorSets.colors[index]) {
                $store.currentColorSets.colors.splice(index, Number.MAX_VALUE);
                $store.emit('changeCurrentColorSets');
            }
        }
    }, {
        key: '/clearPalette',
        value: function clearPalette($store) {
            if ($store.currentColorSets.colors) {
                $store.currentColorSets.colors = [];
                $store.emit('changeCurrentColorSets');
            }
        }
    }, {
        key: '/getCurrentColors',
        value: function getCurrentColors($store) {
            return $store.dispatch('/getColors', $store.currentColorSets);
        }
    }, {
        key: '/getColors',
        value: function getColors($store, element) {
            if (element.scale) {
                return Color$1.scale(element.scale, element.count);
            }

            return element.colors || [];
        }
    }, {
        key: '/getColorSetsList',
        value: function getColorSetsList($store) {
            return $store.dispatch('/list').map(function (element) {
                return {
                    name: element.name,
                    edit: element.edit,
                    colors: $store.dispatch('/getColors', element)
                };
            });
        }
    }]);
    return ColorSetsList;
}(BaseModule);

var Event = {
    addEvent: function addEvent(dom, eventName, callback, options) {
        if (dom) {
            dom.addEventListener(eventName, callback, options);
        }
    },
    removeEvent: function removeEvent(dom, eventName, callback) {
        if (dom) {
            dom.removeEventListener(eventName, callback);
        }
    },
    pos: function pos(e) {
        if (e.touches && e.touches[0]) {
            return e.touches[0];
        }

        return e;
    },
    posXY: function posXY(e) {
        var pos = this.pos(e);
        return {
            x: pos.pageX,
            y: pos.pageY
        };
    }
};

var DELEGATE_SPLIT = '.';

var State = function () {
  function State(masterObj) {
    var settingObj = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
    classCallCheck(this, State);


    this.masterObj = masterObj;
    this.settingObj = settingObj;
  }

  createClass(State, [{
    key: 'set',
    value: function set$$1(key, value) {
      var defaultValue = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : undefined;

      this.settingObj[key] = value || defaultValue;
    }
  }, {
    key: 'init',
    value: function init(key) {

      if (!this.has(key)) {

        var arr = key.split(DELEGATE_SPLIT);

        var obj = this.masterObj.refs[arr[0]] || this.masterObj[arr[0]] || this.masterObj;
        var method = arr.pop();

        if (obj[method]) {
          for (var _len = arguments.length, args = Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
            args[_key - 1] = arguments[_key];
          }

          var value = obj[method].apply(obj, args);

          this.set(key, value);
        }
      }
    }
  }, {
    key: 'get',
    value: function get$$1(key) {
      var defaultValue = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : '';


      this.init(key, defaultValue);

      return this.settingObj[key] || defaultValue;
    }
  }, {
    key: 'has',
    value: function has(key) {
      return !!this.settingObj[key];
    }
  }]);
  return State;
}();

var CHECK_EVENT_PATTERN = /^(click|mouse(down|up|move|enter|leave)|touch(start|move|end)|key(down|up|press)|contextmenu|change|input)/ig;
var CHECK_LOAD_PATTERN = /^load (.*)/ig;
var EVENT_SAPARATOR = ' ';
var META_KEYS = ['Control', 'Shift', 'Alt', 'Meta'];

var EventMachin = function () {
  function EventMachin() {
    classCallCheck(this, EventMachin);

    this.state = new State(this);
    this.refs = {};

    this.childComponents = this.components();
  }

  /**
   * 자식으로 사용할 컴포넌트를 생성해준다. 
   * 생성 시점에 $store 객체가 자동으로 공유된다. 
   * 모든 데이타는 $store 기준으로 작성한다. 
   */


  createClass(EventMachin, [{
    key: 'newChildComponents',
    value: function newChildComponents() {
      var _this = this;

      var childKeys = Object.keys(this.childComponents);
      childKeys.forEach(function (key) {
        var Component = _this.childComponents[key];

        _this[key] = new Component(_this);
      });
    }

    /**
     * 부모가 정의한 template 과  그 안에서 동작하는 자식 컴포넌트들을 다 합쳐서 
     * 최종 element 를 만들어준다. 
     * 
     * 그리고 자동으로 load 되어질게 있으면 로드 해준다. 
     */

  }, {
    key: 'render',
    value: function render() {
      // 1. 나의 template 을 만들어내고  
      this.$el = this.parseTemplate(this.template());
      this.refs.$el = this.$el;

      // 개별 객체 셋팅하고 
      this.parseTarget();

      // 데이타 로드 하고 
      this.load();

      this.afterRender();
    }
  }, {
    key: 'afterRender',
    value: function afterRender() {}

    /**
     * 자식 컴포넌트로 사용될 객체 정의 
     */

  }, {
    key: 'components',
    value: function components() {
      return {};
    }

    /**
     * Class 기반으로 $el 을 생성하기 위해서 
     * 선언형으로 html 템플릿을 정의한다. 
     * 
     * @param {*} html 
     */

  }, {
    key: 'parseTemplate',
    value: function parseTemplate(html) {
      var _this2 = this;

      var $el = new Dom("div").html(html).firstChild();

      // ref element 정리 
      var refs = $el.findAll('[ref]');

      [].concat(toConsumableArray(refs)).forEach(function (node) {
        var name = node.getAttribute('ref');
        _this2.refs[name] = new Dom(node);
      });

      return $el;
    }

    /**
     * target 으로 지정된 자식 컴포넌트를 대체해준다.
     */

  }, {
    key: 'parseTarget',
    value: function parseTarget() {
      var _this3 = this;

      var $el = this.$el;
      var targets = $el.findAll('[target]');

      [].concat(toConsumableArray(targets)).forEach(function (node) {
        var targetComponentName = node.getAttribute('target');
        var refName = node.getAttribute('ref') || targetComponentName;

        var Component = _this3.childComponents[targetComponentName];
        var instance = new Component(_this3);
        _this3[refName] = instance;
        _this3.refs[refName] = instance.$el;

        if (instance) {
          instance.render();
          var $parent = new Dom(node.parentNode);
          $parent.replace(node, instance.$el.el);
        }
      });
    }

    // load function이 정의된 객체는 load 를 실행해준다. 

  }, {
    key: 'load',
    value: function load() {
      var _this4 = this;

      this.filterProps(CHECK_LOAD_PATTERN).forEach(function (callbackName) {
        var elName = callbackName.split('load ')[1];

        if (_this4.refs[elName]) {
          _this4.refs[elName].html(_this4.parseTemplate(_this4[callbackName].call(_this4)));
        }
      });
    }

    // 기본 템플릿 지정 

  }, {
    key: 'template',
    value: function template() {
      return '<div></div>';
    }
  }, {
    key: 'initialize',
    value: function initialize() {}

    /**
     * 이벤트를 초기화한다. 
     */

  }, {
    key: 'initializeEvent',
    value: function initializeEvent() {
      var _this5 = this;

      this.initializeEventMachin();

      // 자식 이벤트도 같이 초기화 한다. 
      // 그래서 이 메소드는 부모에서 한번만 불려도 된다. 
      Object.keys(this.childComponents).forEach(function (key) {
        if (_this5[key]) _this5[key].initializeEvent();
      });
    }

    /**
     * 자원을 해제한다. 
     * 이것도 역시 자식 컴포넌트까지 제어하기 때문에 가장 최상위 부모에서 한번만 호출되도 된다. 
     */

  }, {
    key: 'destroy',
    value: function destroy() {
      var _this6 = this;

      this.destroyEventMachin();
      // this.refs = {} 

      Object.keys(this.childComponents).forEach(function (key) {
        if (_this6[key]) _this6[key].destroy();
      });
    }
  }, {
    key: 'destroyEventMachin',
    value: function destroyEventMachin() {
      this.removeEventAll();
    }
  }, {
    key: 'initializeEventMachin',
    value: function initializeEventMachin() {
      this.filterProps(CHECK_EVENT_PATTERN).forEach(this.parseEvent.bind(this));
    }

    /**
     * property 수집하기 
     * 상위 클래스의 모든 property 를 수집해서 리턴한다. 
     */

  }, {
    key: 'collectProps',
    value: function collectProps() {

      if (!this.collapsedProps) {
        var p = this.__proto__;
        var results = [];
        do {
          results.push.apply(results, toConsumableArray(Object.getOwnPropertyNames(p)));
          p = p.__proto__;
        } while (p);

        this.collapsedProps = results;
      }

      return this.collapsedProps;
    }
  }, {
    key: 'filterProps',
    value: function filterProps(pattern) {
      return this.collectProps().filter(function (key) {
        return key.match(pattern);
      });
    }
  }, {
    key: 'parseEvent',
    value: function parseEvent(key) {
      var arr = key.split(EVENT_SAPARATOR);

      this.bindingEvent(arr, this[key].bind(this));
    }
  }, {
    key: 'getDefaultDomElement',
    value: function getDefaultDomElement(dom) {
      var el = void 0;

      if (dom) {
        el = this.refs[dom] || this[dom] || window[dom];
      } else {
        el = this.el || this.$el || this.$root;
      }

      if (el instanceof Dom) {
        return el.getElement();
      }

      return el;
    }
  }, {
    key: 'getDefaultEventObject',
    value: function getDefaultEventObject(eventName) {
      var _this7 = this;

      var arr = eventName.split('.');
      var realEventName = arr.shift();

      var isControl = arr.includes('Control');
      var isShift = arr.includes('Shift');
      var isAlt = arr.includes('Alt');
      var isMeta = arr.includes('Meta');

      arr = arr.filter(function (code) {
        return META_KEYS.includes(code) === false;
      });

      var checkMethodList = arr.filter(function (code) {
        return !!_this7[code];
      });

      arr = arr.filter(function (code) {
        return checkMethodList.includes(code) === false;
      }).map(function (code) {
        return code.toLowerCase();
      });

      return {
        eventName: realEventName,
        isControl: isControl,
        isShift: isShift,
        isAlt: isAlt,
        isMeta: isMeta,
        codes: arr,
        checkMethodList: checkMethodList
      };
    }
  }, {
    key: 'bindingEvent',
    value: function bindingEvent(_ref, callback) {
      var _ref2 = toArray(_ref),
          eventName = _ref2[0],
          dom = _ref2[1],
          delegate = _ref2.slice(2);

      dom = this.getDefaultDomElement(dom);
      var eventObject = this.getDefaultEventObject(eventName);

      eventObject.dom = dom;
      eventObject.delegate = delegate.join(EVENT_SAPARATOR);

      this.addEvent(eventObject, callback);
    }
  }, {
    key: 'matchPath',
    value: function matchPath(el, selector) {
      if (el) {
        if (el.matches(selector)) {
          return el;
        }
        return this.matchPath(el.parentElement, selector);
      }
      return null;
    }
  }, {
    key: 'getBindings',
    value: function getBindings() {

      if (!this._bindings) {
        this.initBindings();
      }

      return this._bindings;
    }
  }, {
    key: 'addBinding',
    value: function addBinding(obj) {
      this.getBindings().push(obj);
    }
  }, {
    key: 'initBindings',
    value: function initBindings() {
      this._bindings = [];
    }
  }, {
    key: 'checkEventType',
    value: function checkEventType(e, eventObject) {
      var _this8 = this;

      var onlyControl = eventObject.isControl ? e.ctrlKey : true;
      var onlyShift = eventObject.isShift ? e.shiftKey : true;
      var onlyAlt = eventObject.isAlt ? e.altKey : true;
      var onlyMeta = eventObject.isMeta ? e.metaKey : true;

      var hasKeyCode = true;
      if (eventObject.codes.length) {
        hasKeyCode = eventObject.codes.includes(e.code.toLowerCase()) || eventObject.codes.includes(e.key.toLowerCase());
      }

      var isAllCheck = true;
      if (eventObject.checkMethodList.length) {
        // 체크 메소드들은 모든 메소드를 다 적용해야한다. 
        isAllCheck = eventObject.checkMethodList.every(function (method) {
          return _this8[method].call(_this8, e);
        });
      }

      return onlyControl && onlyAlt && onlyShift && onlyMeta && hasKeyCode && isAllCheck;
    }
  }, {
    key: 'makeCallback',
    value: function makeCallback(eventObject, callback) {
      var _this9 = this;

      if (eventObject.delegate) {
        return function (e) {
          e.xy = Event.posXY(e);
          if (_this9.checkEventType(e, eventObject)) {
            var delegateTarget = _this9.matchPath(e.target || e.srcElement, eventObject.delegate);

            if (delegateTarget) {
              // delegate target 이 있는 경우만 callback 실행 
              e.delegateTarget = delegateTarget;
              e.$delegateTarget = new Dom(delegateTarget);
              return callback(e);
            }
          }
        };
      } else {
        return function (e) {
          e.xy = Event.posXY(e);
          if (_this9.checkEventType(e, eventObject)) {
            return callback(e);
          }
        };
      }
    }
  }, {
    key: 'addEvent',
    value: function addEvent(eventObject, callback) {
      eventObject.callback = this.makeCallback(eventObject, callback);
      this.addBinding(eventObject);

      var options = true;
      if (eventObject.eventName === 'touchstart') {
        options = { passive: true };
      }

      Event.addEvent(eventObject.dom, eventObject.eventName, eventObject.callback, options);
    }
  }, {
    key: 'removeEventAll',
    value: function removeEventAll() {
      var _this10 = this;

      this.getBindings().forEach(function (obj) {
        _this10.removeEvent(obj);
      });
      this.initBindings();
    }
  }, {
    key: 'removeEvent',
    value: function removeEvent(_ref3) {
      var eventName = _ref3.eventName,
          dom = _ref3.dom,
          callback = _ref3.callback;

      Event.removeEvent(dom, eventName, callback);
    }
  }]);
  return EventMachin;
}();

var CHECK_STORE_EVENT_PATTERN = /^@/;

var UIElement = function (_EventMachin) {
    inherits(UIElement, _EventMachin);

    function UIElement(opt) {
        classCallCheck(this, UIElement);

        var _this = possibleConstructorReturn(this, (UIElement.__proto__ || Object.getPrototypeOf(UIElement)).call(this, opt));

        _this.opt = opt || {};

        if (opt && opt.$store) {
            _this.$store = opt.$store;
        }

        _this.initialize();

        _this.initializeStoreEvent();
        return _this;
    }

    /**
     * initialize store event 
     * 
     * you can define '@xxx' method(event) in UIElement 
     * 
     * 
     */


    createClass(UIElement, [{
        key: 'initializeStoreEvent',
        value: function initializeStoreEvent() {
            var _this2 = this;

            this.storeEvents = {};
            this.filterProps(CHECK_STORE_EVENT_PATTERN).forEach(function (key) {
                var arr = key.split('@');
                arr.shift();
                var event = arr.join('@');

                _this2.storeEvents[event] = _this2[key].bind(_this2);
                _this2.$store.on(event, _this2.storeEvents[event]);
            });
        }
    }, {
        key: 'destoryStoreEvent',
        value: function destoryStoreEvent() {
            var _this3 = this;

            Object.keys(this.storeEvents).forEach(function (event) {
                _this3.$store.off(event, _this3.storeEvents[event]);
            });
        }
    }]);
    return UIElement;
}(EventMachin);

function isUndefined(v) {
    return typeof v == 'undefined' || v == null;
}

var ColorManager = function (_BaseModule) {
    inherits(ColorManager, _BaseModule);

    function ColorManager() {
        classCallCheck(this, ColorManager);
        return possibleConstructorReturn(this, (ColorManager.__proto__ || Object.getPrototypeOf(ColorManager)).apply(this, arguments));
    }

    createClass(ColorManager, [{
        key: 'initialize',
        value: function initialize() {
            get(ColorManager.prototype.__proto__ || Object.getPrototypeOf(ColorManager.prototype), 'initialize', this).call(this);

            this.$store.rgb = {};
            this.$store.hsl = {};
            this.$store.hsv = {};
            this.$store.alpha = 1;
            this.$store.format = 'hex';

            // this.$store.dispatch('/changeColor');
        }
    }, {
        key: '/changeFormat',
        value: function changeFormat($store, format) {
            $store.format = format;

            $store.emit('changeFormat');
        }
    }, {
        key: '/initColor',
        value: function initColor($store, colorObj, source) {
            $store.dispatch('/changeColor', colorObj, source, true);
            $store.emit('initColor');
        }
    }, {
        key: '/changeColor',
        value: function changeColor($store, colorObj, source, isNotEmit) {

            colorObj = colorObj || '#FF0000';

            if (typeof colorObj == 'string') {
                colorObj = Color$1.parse(colorObj);
            }

            colorObj.source = colorObj.source || source;

            $store.alpha = isUndefined(colorObj.a) ? $store.alpha : colorObj.a;
            $store.format = colorObj.type != 'hsv' ? colorObj.type || $store.format : $store.format;

            if (colorObj.type == 'hsl') {
                $store.hsl = Object.assign($store.hsl, colorObj);
                $store.rgb = Color$1.HSLtoRGB($store.hsl);
                $store.hsv = Color$1.HSLtoHSV(colorObj);
            } else if (colorObj.type == 'hex') {
                $store.rgb = Object.assign($store.rgb, colorObj);
                $store.hsl = Color$1.RGBtoHSL($store.rgb);
                $store.hsv = Color$1.RGBtoHSV(colorObj);
            } else if (colorObj.type == 'rgb') {
                $store.rgb = Object.assign($store.rgb, colorObj);
                $store.hsl = Color$1.RGBtoHSL($store.rgb);
                $store.hsv = Color$1.RGBtoHSV(colorObj);
            } else if (colorObj.type == 'hsv') {
                $store.hsv = Object.assign($store.hsv, colorObj);
                $store.rgb = Color$1.HSVtoRGB($store.hsv);
                $store.hsl = Color$1.HSVtoHSL($store.hsv);
            }

            if (!isNotEmit) {
                $store.emit('changeColor', colorObj.source);
            }
        }
    }, {
        key: '/getHueColor',
        value: function getHueColor($store) {
            return HueColor.checkHueColor($store.hsv.h / 360);
        }
    }, {
        key: '/toString',
        value: function toString($store, type) {
            type = type || $store.format;
            var colorObj = $store[type] || $store.rgb;
            return Color$1.format(_extends({}, colorObj, {
                a: $store.alpha
            }), type);
        }
    }, {
        key: '/toColor',
        value: function toColor($store, type) {
            type = type || $store.format;

            if (type == 'rgb') {
                return $store.dispatch('/toRGB');
            } else if (type == 'hsl') {
                return $store.dispatch('/toHSL');
            } else if (type == 'hex') {
                return $store.dispatch('/toHEX');
            }

            return $store.dispatch('/toString', type);
        }
    }, {
        key: '/toRGB',
        value: function toRGB($store) {
            return $store.dispatch('/toString', 'rgb');
        }
    }, {
        key: '/toHSL',
        value: function toHSL($store) {
            return $store.dispatch('/toString', 'hsl');
        }
    }, {
        key: '/toHEX',
        value: function toHEX($store) {
            return $store.dispatch('/toString', 'hex').toUpperCase();
        }
    }]);
    return ColorManager;
}(BaseModule);

var BaseStore = function () {
    function BaseStore(opt) {
        classCallCheck(this, BaseStore);

        this.callbacks = [];
        this.actions = [];
        this.modules = opt.modules || [];

        this.initialize();
    }

    createClass(BaseStore, [{
        key: 'initialize',
        value: function initialize() {
            this.initializeModule();
        }
    }, {
        key: 'initializeModule',
        value: function initializeModule() {
            var _this = this;

            this.modules.forEach(function (Module) {
                var instance = new Module(_this);
            });
        }
    }, {
        key: 'action',
        value: function action(_action, context) {
            this.actions[_action] = { context: context, callback: context[_action] };
        }
    }, {
        key: 'dispatch',
        value: function dispatch(action) {
            var args = [].concat(Array.prototype.slice.call(arguments));
            var action = args.shift();

            var m = this.actions[action];

            if (m) {
                return m.callback.apply(m.context, [this].concat(toConsumableArray(args)));
            }
        }
    }, {
        key: 'module',
        value: function module(ModuleObject) {
            // this.action()
        }
    }, {
        key: 'on',
        value: function on(event, callback) {
            this.callbacks.push({ event: event, callback: callback });
        }
    }, {
        key: 'off',
        value: function off(event, callback) {

            if (arguments.length == 0) {
                this.callbacks = [];
            } else if (arguments.length == 1) {
                this.callbacks = this.callbacks.filter(function (f) {
                    return f.event != event;
                });
            } else if (arguments.length == 2) {
                this.callbacks = this.callbacks.filter(function (f) {
                    return f.event != event && f.callback != callback;
                });
            }
        }
    }, {
        key: 'emit',
        value: function emit() {
            var args = [].concat(Array.prototype.slice.call(arguments));
            var event = args.shift();

            this.callbacks.filter(function (f) {
                return f.event == event;
            }).forEach(function (f) {
                if (f && typeof f.callback == 'function') {
                    f.callback.apply(f, toConsumableArray(args));
                }
            });
        }
    }]);
    return BaseStore;
}();

var BaseColorPicker = function (_UIElement) {
    inherits(BaseColorPicker, _UIElement);

    function BaseColorPicker(opt) {
        classCallCheck(this, BaseColorPicker);

        var _this = possibleConstructorReturn(this, (BaseColorPicker.__proto__ || Object.getPrototypeOf(BaseColorPicker)).call(this, opt));

        _this.isColorPickerShow = false;
        _this.isShortCut = false;
        _this.hideDelay = +(typeof _this.opt.hideDeplay == 'undefined' ? 2000 : _this.opt.hideDelay);
        _this.timerCloseColorPicker;
        _this.autoHide = _this.opt.autoHide || true;
        _this.outputFormat = _this.opt.outputFormat;
        _this.$checkColorPickerClass = _this.checkColorPickerClass.bind(_this);

        return _this;
    }

    createClass(BaseColorPicker, [{
        key: 'initialize',
        value: function initialize() {
            var _this2 = this;

            this.$body = null;
            this.$root = null;

            this.$store = new BaseStore({
                modules: [ColorManager, ColorSetsList]
            });

            this.callbackChange = function () {
                _this2.callbackColorValue();
            };

            this.callbackLastUpdate = function () {
                _this2.callbackLastUpdateColorValue();
            };

            this.colorpickerShowCallback = function () {};
            this.colorpickerHideCallback = function () {};
            this.colorpickerLastUpdateCallback = function () {};

            this.$body = new Dom(this.getContainer());
            this.$root = new Dom('div', 'codemirror-colorpicker');

            //  append colorpicker to container (ex : body)
            if (this.opt.position == 'inline') {
                this.$body.append(this.$root);
            }

            if (this.opt.type) {
                // to change css style
                this.$root.addClass(this.opt.type);
            }

            if (this.opt.hideInformation) {
                this.$root.addClass('hide-information');
            }

            if (this.opt.hideColorsets) {
                this.$root.addClass('hide-colorsets');
            }

            this.$arrow = new Dom('div', 'arrow');

            this.$root.append(this.$arrow);

            this.$store.dispatch('/setUserPalette', this.opt.colorSets);

            this.render();

            this.$root.append(this.$el);

            this.initColorWithoutChangeEvent(this.opt.color);

            // 이벤트 연결 
            this.initializeEvent();
        }
    }, {
        key: 'initColorWithoutChangeEvent',
        value: function initColorWithoutChangeEvent(color) {
            this.$store.dispatch('/initColor', color);
        }

        /** 
         * public method 
         * 
         */

        /**
         * 
         * show colorpicker with position  
         * 
         * @param {{left, top, hideDelay, isShortCut}} opt 
         * @param {String|Object} color  
         * @param {Function} showCallback  it is called when colorpicker is shown
         * @param {Function} hideCallback  it is called once when colorpicker is hidden
         */

    }, {
        key: 'show',
        value: function show(opt, color, showCallback, hideCallback, lastUpdateCallback) {

            // 매번 이벤트를 지우고 다시 생성할 필요가 없어서 초기화 코드는 지움. 
            // this.destroy();
            // this.initializeEvent();
            // define colorpicker callback
            this.colorpickerShowCallback = showCallback;
            this.colorpickerHideCallback = hideCallback;
            this.colorpickerLastUpdateCallback = lastUpdateCallback;
            this.$root.css(this.getInitalizePosition()).show();

            this.isColorPickerShow = true;
            this.isShortCut = opt.isShortCut || false;
            this.outputFormat = opt.outputFormat;

            // define hide delay
            this.hideDelay = +(typeof opt.hideDelay == 'undefined' ? 2000 : opt.hideDelay);
            if (this.hideDelay > 0) {
                this.setHideDelay(this.hideDelay);
            }

            this.$root.appendTo(this.$body);
            this.definePosition(opt);
            this.initColorWithoutChangeEvent(color);
        }

        /**
         * 
         * initialize color for colorpicker
         * 
         * @param {String|Object} newColor 
         * @param {String} format  hex, rgb, hsl
         */

    }, {
        key: 'initColor',
        value: function initColor(newColor, format) {
            this.$store.dispatch('/changeColor', newColor, format);
        }

        /**
         * hide colorpicker 
         * 
         */

    }, {
        key: 'hide',
        value: function hide() {
            if (this.isColorPickerShow) {
                // this.destroy();
                this.$root.hide();
                this.$root.remove(); // not empty 
                this.isColorPickerShow = false;

                this.callbackHideColorValue();
            }
        }

        /**
         * set to colors in current sets that you see 
         * @param {Array} colors 
         */

    }, {
        key: 'setColorsInPalette',
        value: function setColorsInPalette() {
            var colors = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : [];

            this.$store.dispatch('/setCurrentColorAll', colors);
        }

        /**
         * refresh all color palette 
         * 
         * @param {*} list 
         */

    }, {
        key: 'setUserPalette',
        value: function setUserPalette() {
            var list = arguments.length > 0 && arguments[0] !== undefined ? arguments[0] : [];

            this.$store.dispatch('/setUserPalette', list);
        }

        /**
         * private method 
         */

    }, {
        key: 'getOption',
        value: function getOption(key) {
            return this.opt[key];
        }
    }, {
        key: 'setOption',
        value: function setOption(key, value) {
            this.opt[key] = value;
        }
    }, {
        key: 'isType',
        value: function isType(key) {
            return this.getOption('type') == key;
        }
    }, {
        key: 'isPaletteType',
        value: function isPaletteType() {
            return this.isType('palette');
        }
    }, {
        key: 'isSketchType',
        value: function isSketchType() {
            return this.isType('sketch');
        }
    }, {
        key: 'getContainer',
        value: function getContainer() {
            return this.opt.container || document.body;
        }
    }, {
        key: 'getColor',
        value: function getColor(type) {
            return this.$store.dispatch('/toColor', type);
        }
    }, {
        key: 'definePositionForArrow',
        value: function definePositionForArrow(opt, elementScreenLeft, elementScreenTop) {
            // console.log(arguments)
        }
    }, {
        key: 'definePosition',
        value: function definePosition(opt) {

            var width = this.$root.width();
            var height = this.$root.height();

            // set left position for color picker
            var elementScreenLeft = opt.left - this.$body.scrollLeft();
            if (width + elementScreenLeft > window.innerWidth) {
                elementScreenLeft -= width + elementScreenLeft - window.innerWidth;
            }
            if (elementScreenLeft < 0) {
                elementScreenLeft = 0;
            }

            // set top position for color picker
            var elementScreenTop = opt.top - this.$body.scrollTop();
            if (height + elementScreenTop > window.innerHeight) {
                elementScreenTop -= height + elementScreenTop - window.innerHeight;
            }
            if (elementScreenTop < 0) {
                elementScreenTop = 0;
            }

            // set position
            this.$root.css({
                left: elementScreenLeft + 'px',
                top: elementScreenTop + 'px'
            });

            // this.definePositionForArrow(opt, elementScreenLeft, elementScreenTop);
        }
    }, {
        key: 'getInitalizePosition',
        value: function getInitalizePosition() {
            if (this.opt.position == 'inline') {
                return {
                    position: 'relative',
                    left: 'auto',
                    top: 'auto',
                    display: 'inline-block'
                };
            } else {
                return {
                    position: 'fixed', // color picker has fixed position
                    left: '-10000px',
                    top: '-10000px'
                };
            }
        }
    }, {
        key: 'isAbsolute',
        value: function isAbsolute() {
            return this.opt.position !== 'inline';
        }

        // Event Bindings 

    }, {
        key: 'mouseup.isAbsolute document',
        value: function mouseupIsAbsoluteDocument(e) {

            this.__isMouseDown = false;
            // when color picker clicked in outside
            if (this.checkInHtml(e.target)) {
                //this.setHideDelay(hideDelay);
            } else if (this.checkColorPickerClass(e.target) == false) {
                this.hide();
            } else {
                if (!this.__isMouseIn) {
                    clearTimeout(this.timerCloseColorPicker);
                    this.timerCloseColorPicker = setTimeout(this.hide.bind(this), this.delayTime || this.hideDelay);
                }
            }
        }
    }, {
        key: 'keyup.isAbsolute.escape $root',
        value: function keyupIsAbsoluteEscape$root(e) {
            this.hide();
        }
    }, {
        key: 'mouseover.isAbsolute $root',
        value: function mouseoverIsAbsolute$root(e) {
            clearTimeout(this.timerCloseColorPicker);
            // this.__isMouseDown = true; 
        }
    }, {
        key: 'mousemove.isAbsolute $root',
        value: function mousemoveIsAbsolute$root(e) {
            clearTimeout(this.timerCloseColorPicker);
        }
    }, {
        key: 'mouseenter.isAbsolute $root',
        value: function mouseenterIsAbsolute$root(e) {
            clearTimeout(this.timerCloseColorPicker);
            this.__isMouseIn = true;
        }
    }, {
        key: 'mouseleave.isAbsolute $root',
        value: function mouseleaveIsAbsolute$root(e) {
            this.__isMouseIn = false;
            if (!this.__isMouseDown) {
                clearTimeout(this.timerCloseColorPicker);
                this.timerCloseColorPicker = setTimeout(this.hide.bind(this), this.delayTime || this.hideDelay);
            }
        }
    }, {
        key: 'mousedown.isAbsolute $root',
        value: function mousedownIsAbsolute$root(e) {
            this.__isMouseDown = true;
        }
    }, {
        key: 'setHideDelay',
        value: function setHideDelay(delayTime) {
            this.delayTime = delayTime || 0;
        }
    }, {
        key: 'runHideDelay',
        value: function runHideDelay() {

            if (this.isColorPickerShow) {
                this.setHideDelay();
                // const hideCallback = this.setHideDelay(delayTime);

                // this.timerCloseColorPicker = setTimeout(hideCallback, delayTime);
            }
        }
    }, {
        key: 'callbackColorValue',
        value: function callbackColorValue(color) {
            color = color || this.getCurrentColor();

            if (typeof this.opt.onChange == 'function') {
                this.opt.onChange.call(this, color);
            }

            if (typeof this.colorpickerShowCallback == 'function') {
                this.colorpickerShowCallback(color);
            }
        }
    }, {
        key: 'callbackLastUpdateColorValue',
        value: function callbackLastUpdateColorValue(color) {
            color = color || this.getCurrentColor();

            if (typeof this.opt.onLastUpdate == 'function') {
                this.opt.onLastUpdate.call(this, color);
            }

            if (typeof this.colorpickerLastUpdateCallback == 'function') {
                this.colorpickerLastUpdateCallback(color);
            }
        }
    }, {
        key: 'callbackHideColorValue',
        value: function callbackHideColorValue(color) {
            color = color || this.getCurrentColor();
            if (typeof this.opt.onHide == 'function') {
                this.opt.onHide.call(this, color);
            }

            if (typeof this.colorpickerHideCallback == 'function') {
                this.colorpickerHideCallback(color);
            }
        }
    }, {
        key: 'getCurrentColor',
        value: function getCurrentColor() {
            return this.$store.dispatch('/toColor', this.outputFormat);
        }
    }, {
        key: 'checkColorPickerClass',
        value: function checkColorPickerClass(el) {
            var hasColorView = new Dom(el).closest('codemirror-colorview');
            var hasColorPicker = new Dom(el).closest('codemirror-colorpicker');
            var hasCodeMirror = new Dom(el).closest('CodeMirror');
            var IsInHtml = el.nodeName == 'HTML';

            return !!(hasColorPicker || hasColorView || hasCodeMirror);
        }
    }, {
        key: 'checkInHtml',
        value: function checkInHtml(el) {
            var IsInHtml = el.nodeName == 'HTML';

            return IsInHtml;
        }
    }, {
        key: 'initializeStoreEvent',
        value: function initializeStoreEvent() {
            get(BaseColorPicker.prototype.__proto__ || Object.getPrototypeOf(BaseColorPicker.prototype), 'initializeStoreEvent', this).call(this);

            this.$store.on('changeColor', this.callbackChange);
            this.$store.on('lastUpdateColor', this.callbackLastUpdate);
            this.$store.on('changeFormat', this.callbackChange);
        }
    }, {
        key: 'destroy',
        value: function destroy() {
            get(BaseColorPicker.prototype.__proto__ || Object.getPrototypeOf(BaseColorPicker.prototype), 'destroy', this).call(this);

            this.$store.off('changeColor', this.callbackChange);
            this.$store.off('lastUpdateColor', this.callbackLastUpdate);
            this.$store.off('changeFormat', this.callbackChange);

            this.callbackChange = undefined;
            this.callbackLastUpdate = undefined;

            // remove color picker callback
            this.colorpickerShowCallback = undefined;
            this.colorpickerHideCallback = undefined;
        }
    }]);
    return BaseColorPicker;
}(UIElement);

var BaseBox = function (_UIElement) {
    inherits(BaseBox, _UIElement);

    function BaseBox(opt) {
        classCallCheck(this, BaseBox);

        var _this = possibleConstructorReturn(this, (BaseBox.__proto__ || Object.getPrototypeOf(BaseBox)).call(this, opt));

        _this.source = 'base-box';
        return _this;
    }

    createClass(BaseBox, [{
        key: 'refresh',
        value: function refresh() {}
    }, {
        key: 'refreshColorUI',
        value: function refreshColorUI(e) {}

        /** push change event  */

    }, {
        key: 'changeColor',
        value: function changeColor(opt) {
            this.$store.dispatch('/changeColor', Object.assign({
                source: this.source
            }, opt || {}));
        }

        // Event Bindings 

    }, {
        key: 'mouseup document',
        value: function mouseupDocument(e) {
            this.onDragEnd(e);
        }
    }, {
        key: 'mousemove document',
        value: function mousemoveDocument(e) {
            this.onDragMove(e);
        }
    }, {
        key: 'mousedown $bar',
        value: function mousedown$bar(e) {
            e.preventDefault();
            this.isDown = true;
        }
    }, {
        key: 'mousedown $container',
        value: function mousedown$container(e) {
            this.isDown = true;
            this.onDragStart(e);
        }
    }, {
        key: 'touchend document',
        value: function touchendDocument(e) {
            this.onDragEnd(e);
        }
    }, {
        key: 'touchmove document',
        value: function touchmoveDocument(e) {
            this.onDragMove(e);
        }
    }, {
        key: 'touchstart $bar',
        value: function touchstart$bar(e) {
            e.preventDefault();
            this.isDown = true;
        }
    }, {
        key: 'touchstart $container',
        value: function touchstart$container(e) {
            this.onDragStart(e);
        }
    }, {
        key: 'onDragStart',
        value: function onDragStart(e) {
            this.isDown = true;
            this.refreshColorUI(e);
        }
    }, {
        key: 'onDragMove',
        value: function onDragMove(e) {
            if (this.isDown) {
                this.refreshColorUI(e);
            }
        }

        /* called when mouse is ended move  */

    }, {
        key: 'onDragEnd',
        value: function onDragEnd(e) {
            if (this.isDown) {
                this.$store.emit('lastUpdateColor');
                this.isDown = false;
            }
        }
    }, {
        key: '@changeColor',
        value: function changeColor(sourceType) {
            if (this.source != sourceType) {
                this.refresh();
            }
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }]);
    return BaseBox;
}(UIElement);

var BaseSlider = function (_BaseBox) {
    inherits(BaseSlider, _BaseBox);

    function BaseSlider(opt) {
        classCallCheck(this, BaseSlider);

        var _this = possibleConstructorReturn(this, (BaseSlider.__proto__ || Object.getPrototypeOf(BaseSlider)).call(this, opt));

        _this.minValue = 0; // min domain value 
        _this.maxValue = 1; // max domain value 
        _this.source = 'base-slider';
        return _this;
    }

    /* slider container's min and max position */


    createClass(BaseSlider, [{
        key: 'getMinMaxPosition',
        value: function getMinMaxPosition() {
            var min = this.getMinPosition();
            var width = this.getMaxDist();
            var max = min + width;

            return { min: min, max: max, width: width };
        }

        /** get current position on page  */

    }, {
        key: 'getCurrent',
        value: function getCurrent(value) {
            return min + this.getMaxDist() * value;
        }

        /** get min position on slider container  */

    }, {
        key: 'getMinPosition',
        value: function getMinPosition() {
            return this.refs.$container.offset().left;
        }
    }, {
        key: 'getMaxDist',
        value: function getMaxDist() {
            return this.state.get('$container.width');
        }

        /** get dist for position value */

    }, {
        key: 'getDist',
        value: function getDist(current) {
            var _getMinMaxPosition = this.getMinMaxPosition(),
                min = _getMinMaxPosition.min,
                max = _getMinMaxPosition.max;

            var dist;
            if (current < min) {
                dist = 0;
            } else if (current > max) {
                dist = 100;
            } else {
                dist = (current - min) / (max - min) * 100;
            }

            return dist;
        }

        /** get caculated dist for domain value   */

    }, {
        key: 'getCaculatedDist',
        value: function getCaculatedDist(e) {
            var current = e ? this.getMousePosition(e) : this.getCurrent(this.getDefaultValue() / this.maxValue);
            var dist = this.getDist(current);

            return dist;
        }

        /** get default value used in slider container */

    }, {
        key: 'getDefaultValue',
        value: function getDefaultValue() {
            return 0;
        }

        /** set mosue position */

    }, {
        key: 'setMousePosition',
        value: function setMousePosition(x) {
            this.refs.$bar.css({ left: x + 'px' });
        }

        /** set mouse position in page */

    }, {
        key: 'getMousePosition',
        value: function getMousePosition(e) {
            return Event.pos(e).pageX;
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setColorUI();
        }

        /** set drag bar position  */

    }, {
        key: 'setColorUI',
        value: function setColorUI(v) {

            v = v || this.getDefaultValue();

            if (v <= this.minValue) {
                this.refs.$bar.addClass('first').removeClass('last');
            } else if (v >= this.maxValue) {
                this.refs.$bar.addClass('last').removeClass('first');
            } else {
                this.refs.$bar.removeClass('last').removeClass('first');
            }

            this.setMousePosition(this.getMaxDist() * ((v || 0) / this.maxValue));
        }
    }]);
    return BaseSlider;
}(BaseBox);

var Value = function (_BaseSlider) {
    inherits(Value, _BaseSlider);

    function Value(opt) {
        classCallCheck(this, Value);

        var _this = possibleConstructorReturn(this, (Value.__proto__ || Object.getPrototypeOf(Value)).call(this, opt));

        _this.minValue = 0;
        _this.maxValue = 1;
        _this.source = 'value-control';
        return _this;
    }

    createClass(Value, [{
        key: 'template',
        value: function template() {
            return '\n            <div class="value">\n                <div ref="$container" class="value-container">\n                    <div ref="$bar" class="drag-bar"></div>\n                </div>\n            </div>\n        ';
        }
    }, {
        key: 'setBackgroundColor',
        value: function setBackgroundColor() {
            this.refs.$container.css("background-color", this.$store.dispatch('/toRGB'));
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            get(Value.prototype.__proto__ || Object.getPrototypeOf(Value.prototype), 'refresh', this).call(this);
            this.setBackgroundColor();
        }
    }, {
        key: 'getDefaultValue',
        value: function getDefaultValue() {
            return this.$store.hsv.v;
        }
    }, {
        key: 'refreshColorUI',
        value: function refreshColorUI(e) {
            var dist = this.getCaculatedDist(e);

            this.setColorUI(dist / 100 * this.maxValue);

            this.changeColor({
                type: 'hsv',
                v: dist / 100 * this.maxValue
            });
        }
    }]);
    return Value;
}(BaseSlider);

var Opacity = function (_BaseSlider) {
    inherits(Opacity, _BaseSlider);

    function Opacity(opt) {
        classCallCheck(this, Opacity);

        var _this = possibleConstructorReturn(this, (Opacity.__proto__ || Object.getPrototypeOf(Opacity)).call(this, opt));

        _this.minValue = 0;
        _this.maxValue = 1;
        _this.source = 'opacity-control';
        return _this;
    }

    createClass(Opacity, [{
        key: 'template',
        value: function template() {
            return '\n        <div class="opacity">\n            <div ref="$container" class="opacity-container">\n                <div ref="$colorbar" class="color-bar"></div>\n                <div ref="$bar" class="drag-bar"></div>\n            </div>\n        </div>\n        ';
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            get(Opacity.prototype.__proto__ || Object.getPrototypeOf(Opacity.prototype), 'refresh', this).call(this);
            this.setOpacityColorBar();
        }
    }, {
        key: 'setOpacityColorBar',
        value: function setOpacityColorBar() {
            var rgb = Object.assign({}, this.$store.rgb);

            rgb.a = 0;
            var start = Color$1.format(rgb, 'rgb');

            rgb.a = 1;
            var end = Color$1.format(rgb, 'rgb');

            this.setOpacityColorBarBackground(start, end);
        }
    }, {
        key: 'setOpacityColorBarBackground',
        value: function setOpacityColorBarBackground(start, end) {
            this.refs.$colorbar.css('background', 'linear-gradient(to right, ' + start + ', ' + end + ')');
        }
    }, {
        key: 'getDefaultValue',
        value: function getDefaultValue() {
            return this.$store.alpha;
        }
    }, {
        key: 'refreshColorUI',
        value: function refreshColorUI(e) {
            var dist = this.getCaculatedDist(e);

            this.setColorUI(dist / 100 * this.maxValue);

            this.changeColor({
                a: Math.floor(dist) / 100 * this.maxValue
            });
        }
    }]);
    return Opacity;
}(BaseSlider);

var source = 'macos-control';

var ColorControl = function (_UIElement) {
    inherits(ColorControl, _UIElement);

    function ColorControl() {
        classCallCheck(this, ColorControl);
        return possibleConstructorReturn(this, (ColorControl.__proto__ || Object.getPrototypeOf(ColorControl)).apply(this, arguments));
    }

    createClass(ColorControl, [{
        key: 'components',
        value: function components() {
            return { Value: Value, Opacity: Opacity };
        }
    }, {
        key: 'template',
        value: function template() {
            return '\n        <div class="control">\n            <div target="Value" ></div>\n            <div target="Opacity" ></div>\n            <div ref="$controlPattern" class="empty"></div>\n            <div ref="$controlColor" class="color"></div>\n        </div>\n        ';
        }
    }, {
        key: 'setBackgroundColor',
        value: function setBackgroundColor() {
            this.refs.$controlColor.css("background-color", this.$store.dispatch('/toRGB'));
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setColorUI();
            this.setBackgroundColor();
        }
    }, {
        key: 'setColorUI',
        value: function setColorUI() {
            this.Value.setColorUI();
            this.Opacity.setColorUI();
        }
    }, {
        key: '@changeColor',
        value: function changeColor(sourceType) {
            if (source != sourceType) {
                this.refresh();
            }
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }]);
    return ColorControl;
}(UIElement);

var ColorWheel = function (_UIElement) {
    inherits(ColorWheel, _UIElement);

    function ColorWheel(opt) {
        classCallCheck(this, ColorWheel);

        var _this = possibleConstructorReturn(this, (ColorWheel.__proto__ || Object.getPrototypeOf(ColorWheel)).call(this, opt));

        _this.width = 214;
        _this.height = 214;
        _this.thinkness = 0;
        _this.half_thinkness = 0;
        _this.source = 'colorwheel';
        return _this;
    }

    createClass(ColorWheel, [{
        key: 'template',
        value: function template() {
            return '\n        <div class="wheel">\n            <canvas class="wheel-canvas" ref="$colorwheel" ></canvas>\n            <div class="wheel-canvas" ref="$valuewheel" ></div>\n            <div class="drag-pointer" ref="$drag_pointer"></div>\n        </div>\n        ';
        }
    }, {
        key: 'refresh',
        value: function refresh(isEvent) {
            this.setColorUI(isEvent);
        }
    }, {
        key: 'setColorUI',
        value: function setColorUI(isEvent) {
            this.renderCanvas();
            this.renderValue();
            this.setHueColor(null, isEvent);
        }
    }, {
        key: 'renderValue',
        value: function renderValue() {
            var value = 1 - this.$store.hsv.v;
            this.refs.$valuewheel.css({
                'background-color': 'rgba(0, 0, 0, ' + value + ')'
            });
        }
    }, {
        key: 'renderWheel',
        value: function renderWheel(width, height) {

            if (this.width && !width) width = this.width;
            if (this.height && !height) height = this.height;

            var $canvas = new Dom('canvas');
            var context = $canvas.el.getContext('2d');
            $canvas.el.width = width;
            $canvas.el.height = height;
            $canvas.css({ width: width + 'px', height: height + 'px' });

            var img = context.getImageData(0, 0, width, height);
            var pixels = img.data;
            var half_width = Math.floor(width / 2);
            var half_height = Math.floor(height / 2);

            var radius = width > height ? half_height : half_width;
            var cx = half_width;
            var cy = half_height;

            for (var y = 0; y < height; y++) {
                for (var x = 0; x < width; x++) {
                    var rx = x - cx + 1,
                        ry = y - cy + 1,
                        d = rx * rx + ry * ry,
                        hue = caculateAngle(rx, ry);

                    var rgb = Color$1.HSVtoRGB(hue, // 0~360 hue 
                    Math.min(Math.sqrt(d) / radius, 1), // 0..1 Saturation 
                    1 //  0..1 Value
                    );

                    var index = (y * width + x) * 4;
                    pixels[index] = rgb.r;
                    pixels[index + 1] = rgb.g;
                    pixels[index + 2] = rgb.b;
                    pixels[index + 3] = 255;
                }
            }

            context.putImageData(img, 0, 0);

            if (this.thinkness > 0) {
                context.globalCompositeOperation = "destination-out"; // destination-out 은 그리는 영역이 지워진다. 
                context.fillStyle = 'black';
                context.beginPath();
                context.arc(cx, cy, radius - this.thinkness, 0, Math.PI * 2);
                context.closePath();
                context.fill();
            }

            return $canvas;
        }
    }, {
        key: 'renderCanvas',
        value: function renderCanvas() {

            // only once rendering 
            if (this.$store.createdWheelCanvas) return;

            var $canvas = this.refs.$colorwheel;
            // console.log($canvas);
            var context = $canvas.el.getContext('2d');

            var _$canvas$size = $canvas.size(),
                _$canvas$size2 = slicedToArray(_$canvas$size, 2),
                width = _$canvas$size2[0],
                height = _$canvas$size2[1];

            if (this.width && !width) width = this.width;
            if (this.height && !height) height = this.height;

            $canvas.el.width = width;
            $canvas.el.height = height;
            $canvas.css({ width: width + 'px', height: height + 'px' });

            var $wheelCanvas = this.renderWheel(width, height);

            context.drawImage($wheelCanvas.el, 0, 0);

            this.$store.createdWheelCanvas = true;
        }
    }, {
        key: 'getDefaultValue',
        value: function getDefaultValue() {
            return this.$store.hsv.h;
        }
    }, {
        key: 'getDefaultSaturation',
        value: function getDefaultSaturation() {
            return this.$store.hsv.s;
        }
    }, {
        key: 'getCurrentXY',
        value: function getCurrentXY(e, angle, radius, centerX, centerY) {
            return e ? Event.posXY(e) : getXYInCircle(angle, radius, centerX, centerY);
        }
    }, {
        key: 'getRectangle',
        value: function getRectangle() {
            var width = this.state.get('$el.width');
            var height = this.state.get('$el.height');
            var radius = this.state.get('$colorwheel.width') / 2;

            var minX = this.refs.$el.offset().left;
            var centerX = minX + width / 2;

            var minY = this.refs.$el.offset().top;
            var centerY = minY + height / 2;

            return { minX: minX, minY: minY, width: width, height: height, radius: radius, centerX: centerX, centerY: centerY };
        }
    }, {
        key: 'setHueColor',
        value: function setHueColor(e, isEvent) {

            if (!this.state.get('$el.width')) return;

            var _getRectangle = this.getRectangle(),
                minX = _getRectangle.minX,
                minY = _getRectangle.minY,
                radius = _getRectangle.radius,
                centerX = _getRectangle.centerX,
                centerY = _getRectangle.centerY;

            var _getCurrentXY = this.getCurrentXY(e, this.getDefaultValue(), this.getDefaultSaturation() * radius, centerX, centerY),
                x = _getCurrentXY.x,
                y = _getCurrentXY.y;

            var rx = x - centerX,
                ry = y - centerY,
                d = rx * rx + ry * ry,
                hue = caculateAngle(rx, ry);

            if (d > radius * radius) {
                var _getCurrentXY2 = this.getCurrentXY(null, hue, radius, centerX, centerY),
                    x = _getCurrentXY2.x,
                    y = _getCurrentXY2.y;
            }

            // saturation 을 
            var saturation = Math.min(Math.sqrt(d) / radius, 1);

            // set drag pointer position 
            this.refs.$drag_pointer.css({
                left: x - minX + 'px',
                top: y - minY + 'px'
            });

            if (!isEvent) {
                this.changeColor({
                    type: 'hsv',
                    h: hue,
                    s: saturation
                });
            }
        }
    }, {
        key: 'changeColor',
        value: function changeColor(opt) {
            this.$store.dispatch('/changeColor', Object.assign({
                source: this.source
            }, opt || {}));
        }
    }, {
        key: '@changeColor',
        value: function changeColor(sourceType) {
            if (this.source != sourceType) {
                this.refresh(true);
            }
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh(true);
        }

        // Event Bindings 

    }, {
        key: 'mouseup document',
        value: function mouseupDocument(e) {
            if (this.isDown) {
                this.isDown = false;
                this.$store.emit('lastUpdateColor');
            }
        }
    }, {
        key: 'mousemove document',
        value: function mousemoveDocument(e) {
            if (this.isDown) {
                this.setHueColor(e);
            }
        }
    }, {
        key: 'mousedown $drag_pointer',
        value: function mousedown$drag_pointer(e) {
            e.preventDefault();
            this.isDown = true;
        }
    }, {
        key: 'mousedown $el',
        value: function mousedown$el(e) {
            this.isDown = true;
            this.setHueColor(e);
        }
    }, {
        key: 'touchend document',
        value: function touchendDocument(e) {
            if (this.isDown) {
                this.isDown = false;
                this.$store.emit('lastUpdateColor');
            }
        }
    }, {
        key: 'touchmove document',
        value: function touchmoveDocument(e) {
            if (this.isDown) {
                this.setHueColor(e);
            }
        }
    }, {
        key: 'touchstart $drag_pointer',
        value: function touchstart$drag_pointer(e) {
            e.preventDefault();
            this.isDown = true;
        }
    }, {
        key: 'touchstart $el',
        value: function touchstart$el(e) {
            e.preventDefault();
            this.isDown = true;
            this.setHueColor(e);
        }
    }]);
    return ColorWheel;
}(UIElement);

var source$2 = 'chromedevtool-information';

var ColorInformation = function (_UIElement) {
    inherits(ColorInformation, _UIElement);

    function ColorInformation() {
        classCallCheck(this, ColorInformation);
        return possibleConstructorReturn(this, (ColorInformation.__proto__ || Object.getPrototypeOf(ColorInformation)).apply(this, arguments));
    }

    createClass(ColorInformation, [{
        key: 'template',
        value: function template() {
            return (/*html*/'\n        <div class="information hex">\n            <div ref="$informationChange" class="information-change">\n                <button ref="$formatChangeButton" type="button" class="format-change-button arrow-button"></button>\n            </div>\n            <div class="information-item hex">\n                <div class="input-field hex">\n                    <input ref="$hexCode" class="input" type="text" />\n                    <div class="title">HEX</div>\n                </div>\n            </div>\n            <div class="information-item rgb">\n                <div class="input-field rgb-r">\n                    <input ref="$rgb_r" class="input" type="number" step="1" min="0" max="255" />\n                    <div class="title">R</div>\n                </div>\n                <div class="input-field rgb-g">\n                    <input ref="$rgb_g" class="input" type="number" step="1" min="0" max="255" />\n                    <div class="title">G</div>\n                </div>\n                <div class="input-field rgb-b">\n                    <input ref="$rgb_b" class="input" type="number" step="1" min="0" max="255" />\n                    <div class="title">B</div>\n                </div>          \n                <div class="input-field rgb-a">\n                    <input ref="$rgb_a" class="input" type="number" step="0.01" min="0" max="1" />\n                    <div class="title">A</div>\n                </div>                                                            \n            </div>\n            <div class="information-item hsl">\n                <div class="input-field hsl-h">\n                    <input ref="$hsl_h" class="input" type="number" step="1" min="0" max="360" />\n                    <div class="title">H</div>\n                </div>\n                <div class="input-field hsl-s">\n                    <input ref="$hsl_s" class="input" type="number" step="1" min="0" max="100" />\n                    <div class="postfix">%</div>\n                    <div class="title">S</div>\n                </div>\n                <div class="input-field hsl-l">\n                    <input ref="$hsl_l" class="input" type="number" step="1" min="0" max="100" />\n                    <div class="postfix">%</div>                        \n                    <div class="title">L</div>\n                </div>\n                <div class="input-field hsl-a">\n                    <input ref="$hsl_a" class="input" type="number" step="0.01" min="0" max="1" />\n                    <div class="title">A</div>\n                </div>\n            </div>\n        </div>\n        '
            );
        }
    }, {
        key: 'setCurrentFormat',
        value: function setCurrentFormat(format) {
            this.format = format;

            this.initFormat();
        }
    }, {
        key: 'initFormat',
        value: function initFormat() {
            var _this2 = this;

            var current_format = this.format || 'hex';

            ['hex', 'rgb', 'hsl'].filter(function (it) {
                return it !== current_format;
            }).forEach(function (formatString) {
                _this2.$el.removeClass(formatString);
            });

            this.$el.addClass(current_format);
        }
    }, {
        key: 'nextFormat',
        value: function nextFormat() {
            var current_format = this.$store.format || 'hex';

            var next_format = 'hex';
            if (current_format == 'hex') {
                next_format = 'rgb';
            } else if (current_format == 'rgb') {
                next_format = 'hsl';
            } else if (current_format == 'hsl') {
                next_format = 'hex';
            }

            this.format = next_format;
            this.$store.dispatch('/changeFormat', next_format);
            this.$store.emit('lastUpdateColor');
            this.initFormat();
        }
    }, {
        key: 'goToFormat',
        value: function goToFormat(to_format) {
            this.format = to_format;
            this.$store.dispatch('/changeFormat', this.format);
            this.$store.emit('lastUpdateColor');
            this.initFormat();
        }
    }, {
        key: 'getFormat',
        value: function getFormat() {
            return this.format || 'hex';
        }
    }, {
        key: 'checkNumberKey',
        value: function checkNumberKey(e) {
            var code = e.which,
                isExcept = false;

            if (code == 37 || code == 39 || code == 8 || code == 46 || code == 9) isExcept = true;

            if (!isExcept && (code < 48 || code > 57)) return false;

            return true;
        }
    }, {
        key: 'checkNotNumberKey',
        value: function checkNotNumberKey(e) {
            return !this.checkNumberKey(e);
        }
    }, {
        key: 'changeRgbColor',
        value: function changeRgbColor() {
            this.$store.dispatch('/changeColor', {
                type: 'rgb',
                r: this.refs.$rgb_r.int(),
                g: this.refs.$rgb_g.int(),
                b: this.refs.$rgb_b.int(),
                a: this.refs.$rgb_a.float(),
                source: source$2
            });
            this.$store.emit('lastUpdateColor');
        }
    }, {
        key: 'changeHslColor',
        value: function changeHslColor() {
            this.$store.dispatch('/changeColor', {
                type: 'hsl',
                h: this.refs.$hsl_h.int(),
                s: this.refs.$hsl_s.int(),
                l: this.refs.$hsl_l.int(),
                a: this.refs.$hsl_a.float(),
                source: source$2
            });
            this.$store.emit('lastUpdateColor');
        }
    }, {
        key: '@changeColor',
        value: function changeColor(sourceType) {
            if (source$2 != sourceType) {
                this.refresh();
            }
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }, {
        key: 'input $rgb_r',
        value: function input$rgb_r(e) {
            this.changeRgbColor();
        }
    }, {
        key: 'input $rgb_g',
        value: function input$rgb_g(e) {
            this.changeRgbColor();
        }
    }, {
        key: 'input $rgb_b',
        value: function input$rgb_b(e) {
            this.changeRgbColor();
        }
    }, {
        key: 'input $rgb_a',
        value: function input$rgb_a(e) {
            this.changeRgbColor();
        }
    }, {
        key: 'input $hsl_h',
        value: function input$hsl_h(e) {
            this.changeHslColor();
        }
    }, {
        key: 'input $hsl_s',
        value: function input$hsl_s(e) {
            this.changeHslColor();
        }
    }, {
        key: 'input $hsl_l',
        value: function input$hsl_l(e) {
            this.changeHslColor();
        }
    }, {
        key: 'input $hsl_a',
        value: function input$hsl_a(e) {
            this.changeHslColor();
        }
    }, {
        key: 'keyup $hexCode',
        value: function keyup$hexCode(e) {
            var code = this.refs.$hexCode.val();

            if (code.charAt(0) == '#' && (code.length == 7 || code.length === 9)) {
                this.$store.dispatch('/changeColor', code, source$2);
                this.$store.emit('lastUpdateColor');
            }
        }
    }, {
        key: 'click $formatChangeButton',
        value: function click$formatChangeButton(e) {
            this.nextFormat();
        }
    }, {
        key: 'click $el .information-item.hex .input-field .title',
        value: function click$elInformationItemHexInputFieldTitle(e) {
            this.goToFormat('rgb');
        }
    }, {
        key: 'click $el .information-item.rgb .input-field .title',
        value: function click$elInformationItemRgbInputFieldTitle(e) {
            this.goToFormat('hsl');
        }
    }, {
        key: 'click $el .information-item.hsl .input-field .title',
        value: function click$elInformationItemHslInputFieldTitle(e) {
            this.goToFormat('hex');
        }
    }, {
        key: 'setRGBInput',
        value: function setRGBInput() {
            this.refs.$rgb_r.val(this.$store.rgb.r);
            this.refs.$rgb_g.val(this.$store.rgb.g);
            this.refs.$rgb_b.val(this.$store.rgb.b);
            this.refs.$rgb_a.val(this.$store.alpha);
        }
    }, {
        key: 'setHSLInput',
        value: function setHSLInput() {
            this.refs.$hsl_h.val(this.$store.hsl.h);
            this.refs.$hsl_s.val(this.$store.hsl.s);
            this.refs.$hsl_l.val(this.$store.hsl.l);
            this.refs.$hsl_a.val(this.$store.alpha);
        }
    }, {
        key: 'setHexInput',
        value: function setHexInput() {
            this.refs.$hexCode.val(this.$store.dispatch('/toHEX'));
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setCurrentFormat(this.$store.format);
            this.setRGBInput();
            this.setHSLInput();
            this.setHexInput();
        }
    }]);
    return ColorInformation;
}(UIElement);

var DATA_COLORSETS_INDEX = 'data-colorsets-index';

var ColorSetsChooser = function (_UIElement) {
    inherits(ColorSetsChooser, _UIElement);

    function ColorSetsChooser() {
        classCallCheck(this, ColorSetsChooser);
        return possibleConstructorReturn(this, (ColorSetsChooser.__proto__ || Object.getPrototypeOf(ColorSetsChooser)).apply(this, arguments));
    }

    createClass(ColorSetsChooser, [{
        key: 'template',
        value: function template() {
            return '\n            <div class="color-chooser">\n                <div class="color-chooser-container">\n                    <div class="colorsets-item colorsets-item-header">\n                        <h1 class="title">Color Palettes</h1>\n                        <span ref="$toggleButton" class="items">&times;</span>\n                    </div>\n                    <div ref="$colorsetsList" class="colorsets-list"></div>\n                </div>\n            </div>\n        ';
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.load();
        }
    }, {
        key: '@changeCurrentColorSets',
        value: function changeCurrentColorSets() {
            this.refresh();
        }
    }, {
        key: '@toggleColorChooser',
        value: function toggleColorChooser() {
            this.toggle();
        }

        // loadable 

    }, {
        key: 'load $colorsetsList',
        value: function load$colorsetsList() {
            // colorsets 
            var colorSets = this.$store.dispatch('/getColorSetsList');

            return '\n            <div>\n                ' + colorSets.map(function (element, index) {
                return '\n                        <div class="colorsets-item" data-colorsets-index="' + index + '" >\n                            <h1 class="title">' + element.name + '</h1>\n                            <div class="items">\n                                <div>\n                                    ' + element.colors.filter(function (color, i) {
                    return i < 5;
                }).map(function (color) {
                    color = color || 'rgba(255, 255, 255, 1)';
                    return '<div class="color-item" title="' + color + '">\n                                                <div class="color-view" style="background-color: ' + color + '"></div>\n                                            </div>';
                }).join('') + '\n                                </div>\n                            </div>\n                        </div>';
            }).join('') + '\n            </div>\n        ';
        }
    }, {
        key: 'show',
        value: function show() {
            this.$el.addClass('open');
        }
    }, {
        key: 'hide',
        value: function hide() {
            this.$el.removeClass('open');
        }
    }, {
        key: 'toggle',
        value: function toggle() {
            this.$el.toggleClass('open');
        }
    }, {
        key: 'click $toggleButton',
        value: function click$toggleButton(e) {
            this.toggle();
        }
    }, {
        key: 'click $colorsetsList .colorsets-item',
        value: function click$colorsetsListColorsetsItem(e) {
            var $item = e.$delegateTarget;

            if ($item) {

                var index = parseInt($item.attr(DATA_COLORSETS_INDEX));

                this.$store.dispatch('/setCurrentColorSets', index);

                this.hide();
            }
        }
    }, {
        key: 'destroy',
        value: function destroy() {
            get(ColorSetsChooser.prototype.__proto__ || Object.getPrototypeOf(ColorSetsChooser.prototype), 'destroy', this).call(this);

            this.hide();
        }
    }]);
    return ColorSetsChooser;
}(UIElement);

var CurrentColorSets = function (_UIElement) {
    inherits(CurrentColorSets, _UIElement);

    function CurrentColorSets() {
        classCallCheck(this, CurrentColorSets);
        return possibleConstructorReturn(this, (CurrentColorSets.__proto__ || Object.getPrototypeOf(CurrentColorSets)).apply(this, arguments));
    }

    createClass(CurrentColorSets, [{
        key: 'template',
        value: function template() {
            return '\n            <div class="colorsets">\n                <div class="menu" title="Open Color Palettes">\n                    <button ref="$colorSetsChooseButton" type="button" class="color-sets-choose-btn arrow-button"></button>\n                </div>\n                <div ref="$colorSetsColorList" class="color-list"></div>\n            </div>\n        ';
        }
    }, {
        key: 'load $colorSetsColorList',
        value: function load$colorSetsColorList() {
            var currentColorSets = this.$store.dispatch('/getCurrentColorSets');
            var colors = this.$store.dispatch('/getCurrentColors');

            return '\n            <div class="current-color-sets">\n            ' + colors.map(function (color, i) {
                return '<div class="color-item" title="' + color + '" data-index="' + i + '" data-color="' + color + '">\n                    <div class="empty"></div>\n                    <div class="color-view" style="background-color: ' + color + '"></div>\n                </div>';
            }).join('') + '   \n            ' + (currentColorSets.edit ? '<div class="add-color-item">+</div>' : '') + '         \n            </div>\n        ';
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.load();
        }
    }, {
        key: 'addColor',
        value: function addColor(color) {
            this.$store.dispatch('/addCurrentColor', color);
        }
    }, {
        key: '@changeCurrentColorSets',
        value: function changeCurrentColorSets() {
            this.refresh();
        }
    }, {
        key: 'click $colorSetsChooseButton',
        value: function click$colorSetsChooseButton(e) {
            this.$store.emit('toggleColorChooser');
        }
    }, {
        key: 'contextmenu $colorSetsColorList',
        value: function contextmenu$colorSetsColorList(e) {
            e.preventDefault();
            var currentColorSets = this.$store.dispatch('/getCurrentColorSets');

            if (!currentColorSets.edit) {
                return;
            }

            var $target = new Dom(e.target);

            var $item = $target.closest('color-item');

            if ($item) {
                var index = parseInt($item.attr('data-index'));

                this.$store.emit('showContextMenu', e, index);
            } else {
                this.$store.emit('showContextMenu', e);
            }
        }
    }, {
        key: 'click $colorSetsColorList .add-color-item',
        value: function click$colorSetsColorListAddColorItem(e) {
            this.addColor(this.$store.dispatch('/toColor'));
        }
    }, {
        key: 'click $colorSetsColorList .color-item',
        value: function click$colorSetsColorListColorItem(e) {
            this.$store.dispatch('/changeColor', e.$delegateTarget.attr('data-color'));
            this.$store.emit('lastUpdateColor');
        }
    }]);
    return CurrentColorSets;
}(UIElement);

var CurrentColorSetsContextMenu = function (_UIElement) {
    inherits(CurrentColorSetsContextMenu, _UIElement);

    function CurrentColorSetsContextMenu() {
        classCallCheck(this, CurrentColorSetsContextMenu);
        return possibleConstructorReturn(this, (CurrentColorSetsContextMenu.__proto__ || Object.getPrototypeOf(CurrentColorSetsContextMenu)).apply(this, arguments));
    }

    createClass(CurrentColorSetsContextMenu, [{
        key: 'template',
        value: function template() {
            return '\n            <ul class="colorsets-contextmenu">\n                <li class="menu-item small-hide" data-type="remove-color">Remove color</li>\n                <li class="menu-item small-hide" data-type="remove-all-to-the-right">Remove all to the right</li>\n                <li class="menu-item" data-type="clear-palette">Clear palette</li>\n            </ul>\n        ';
        }
    }, {
        key: 'show',
        value: function show(e, index) {
            var $event = Event.pos(e);

            this.$el.css({
                top: $event.clientY - 10 + 'px',
                left: $event.clientX + 'px'
            });
            this.$el.addClass('show');
            this.selectedColorIndex = index;

            if (typeof this.selectedColorIndex == 'undefined') {
                this.$el.addClass('small');
            } else {
                this.$el.removeClass('small');
            }
        }
    }, {
        key: 'hide',
        value: function hide() {
            this.$el.removeClass('show');
        }
    }, {
        key: 'runCommand',
        value: function runCommand(command) {
            switch (command) {
                case 'remove-color':
                    this.$store.dispatch('/removeCurrentColor', this.selectedColorIndex);
                    break;
                case 'remove-all-to-the-right':
                    this.$store.dispatch('/removeCurrentColorToTheRight', this.selectedColorIndex);
                    break;
                case 'clear-palette':
                    this.$store.dispatch('/clearPalette');
                    break;
            }
        }
    }, {
        key: '@showContextMenu',
        value: function showContextMenu(e, index) {
            this.show(e, index);
        }
    }, {
        key: 'click $el .menu-item',
        value: function click$elMenuItem(e) {
            e.preventDefault();

            this.runCommand(e.$delegateTarget.attr('data-type'));
            this.hide();
        }
    }]);
    return CurrentColorSetsContextMenu;
}(UIElement);

var MacOSColorPicker = function (_BaseColorPicker) {
    inherits(MacOSColorPicker, _BaseColorPicker);

    function MacOSColorPicker() {
        classCallCheck(this, MacOSColorPicker);
        return possibleConstructorReturn(this, (MacOSColorPicker.__proto__ || Object.getPrototypeOf(MacOSColorPicker)).apply(this, arguments));
    }

    createClass(MacOSColorPicker, [{
        key: 'template',
        value: function template() {
            return '\n            <div class=\'colorpicker-body\'>\n                <div target="colorwheel"></div>\n                <div target="control"></div>\n                <div target="information"></div>\n                <div target="currentColorSets"></div>\n                <div target="colorSetsChooser"></div>\n                <div target="contextMenu"></div>                \n            </div>\n        ';
        }
    }, {
        key: 'components',
        value: function components() {
            return {
                colorwheel: ColorWheel,
                control: ColorControl,
                information: ColorInformation,
                currentColorSets: CurrentColorSets,
                colorSetsChooser: ColorSetsChooser,
                contextMenu: CurrentColorSetsContextMenu
            };
        }
    }]);
    return MacOSColorPicker;
}(BaseColorPicker);

var Hue = function (_BaseSlider) {
    inherits(Hue, _BaseSlider);

    function Hue(opt) {
        classCallCheck(this, Hue);

        var _this = possibleConstructorReturn(this, (Hue.__proto__ || Object.getPrototypeOf(Hue)).call(this, opt));

        _this.minValue = 0;
        _this.maxValue = 360;
        _this.source = 'hue-control';
        return _this;
    }

    createClass(Hue, [{
        key: 'template',
        value: function template() {
            return '\n            <div class="hue">\n                <div ref="$container" class="hue-container">\n                    <div ref="$bar" class="drag-bar"></div>\n                </div>\n            </div>\n        ';
        }
    }, {
        key: 'getDefaultValue',
        value: function getDefaultValue() {
            return this.$store.hsv.h;
        }
    }, {
        key: 'refreshColorUI',
        value: function refreshColorUI(e) {

            var dist = this.getCaculatedDist(e);

            this.setColorUI(dist / 100 * this.maxValue);

            this.changeColor({
                h: dist / 100 * this.maxValue,
                type: 'hsv'
            });
        }
    }]);
    return Hue;
}(BaseSlider);

var source$3 = 'chromedevtool-control';

var ColorControl$2 = function (_UIElement) {
    inherits(ColorControl, _UIElement);

    function ColorControl() {
        classCallCheck(this, ColorControl);
        return possibleConstructorReturn(this, (ColorControl.__proto__ || Object.getPrototypeOf(ColorControl)).apply(this, arguments));
    }

    createClass(ColorControl, [{
        key: 'components',
        value: function components() {
            return { Hue: Hue, Opacity: Opacity };
        }
    }, {
        key: 'template',
        value: function template() {
            return '\n        <div class="control">\n            <div target="Hue" ></div>\n            <div target="Opacity" ></div>\n            <div ref="$controlPattern" class="empty"></div>\n            <div ref="$controlColor" class="color"></div>\n        </div>\n        ';
        }
    }, {
        key: 'setBackgroundColor',
        value: function setBackgroundColor() {
            this.refs.$controlColor.css("background-color", this.$store.dispatch('/toRGB'));
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setColorUI();
            this.setBackgroundColor();
        }
    }, {
        key: 'setColorUI',
        value: function setColorUI() {
            this.Hue.setColorUI();
            this.Opacity.setColorUI();
        }
    }, {
        key: '@changeColor',
        value: function changeColor(sourceType) {
            if (source$3 != sourceType) {
                this.refresh();
            }
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }]);
    return ColorControl;
}(UIElement);

var source$4 = 'chromedevtool-palette';

var ColorPalette = function (_UIElement) {
    inherits(ColorPalette, _UIElement);

    function ColorPalette() {
        classCallCheck(this, ColorPalette);
        return possibleConstructorReturn(this, (ColorPalette.__proto__ || Object.getPrototypeOf(ColorPalette)).apply(this, arguments));
    }

    createClass(ColorPalette, [{
        key: 'template',
        value: function template() {
            return '\n        <div class="color">\n            <div ref="$saturation" class="saturation">\n                <div ref="$value" class="value">\n                    <div ref="$drag_pointer" class="drag-pointer"></div>\n                </div>\n            </div>        \n        </div>        \n        ';
        }
    }, {
        key: 'setBackgroundColor',
        value: function setBackgroundColor(color) {
            this.$el.css("background-color", color);
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setColorUI();
        }
    }, {
        key: 'caculateSV',
        value: function caculateSV() {
            var pos = this.drag_pointer_pos || { x: 0, y: 0 };

            var width = this.state.get('$el.width');
            var height = this.state.get('$el.height');

            var s = pos.x / width;
            var v = (height - pos.y) / height;

            this.$store.dispatch('/changeColor', {
                type: 'hsv',
                s: s,
                v: v,
                source: source$4
            });
        }
    }, {
        key: 'setColorUI',
        value: function setColorUI() {
            var x = this.state.get('$el.width') * this.$store.hsv.s,
                y = this.state.get('$el.height') * (1 - this.$store.hsv.v);

            this.refs.$drag_pointer.css({
                left: x + "px",
                top: y + "px"
            });

            this.drag_pointer_pos = { x: x, y: y };

            this.setBackgroundColor(this.$store.dispatch('/getHueColor'));
        }
    }, {
        key: 'setMainColor',
        value: function setMainColor(e) {
            // e.preventDefault();
            var pos = this.$el.offset(); // position for screen
            var w = this.state.get('$el.contentWidth');
            var h = this.state.get('$el.contentHeight');

            var x = Event.pos(e).pageX - pos.left;
            var y = Event.pos(e).pageY - pos.top;

            if (x < 0) x = 0;else if (x > w) x = w;

            if (y < 0) y = 0;else if (y > h) y = h;

            this.refs.$drag_pointer.css({
                left: x + 'px',
                top: y + 'px'
            });

            this.drag_pointer_pos = { x: x, y: y };

            this.caculateSV();
        }
    }, {
        key: '@changeColor',
        value: function changeColor(sourceType) {
            if (source$4 != sourceType) {
                this.refresh();
            }
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }, {
        key: 'mouseup document',
        value: function mouseupDocument(e) {
            if (this.isDown) {
                this.isDown = false;
                this.$store.emit('lastUpdateColor');
            }
        }
    }, {
        key: 'mousemove document',
        value: function mousemoveDocument(e) {
            if (this.isDown) {
                this.setMainColor(e);
            }
        }
    }, {
        key: 'mousedown',
        value: function mousedown(e) {
            this.isDown = true;
            this.setMainColor(e);
        }
    }, {
        key: 'touchend document',
        value: function touchendDocument(e) {
            if (this.isDown) {
                this.isDown = false;
                this.$store.emit('lastUpdateColor');
            }
        }
    }, {
        key: 'touchmove document',
        value: function touchmoveDocument(e) {
            if (this.isDown) {
                this.setMainColor(e);
            }
        }
    }, {
        key: 'touchstart',
        value: function touchstart(e) {
            e.preventDefault();
            this.isDown = true;
            this.setMainColor(e);
        }
    }]);
    return ColorPalette;
}(UIElement);

var ChromeDevToolColorPicker = function (_BaseColorPicker) {
    inherits(ChromeDevToolColorPicker, _BaseColorPicker);

    function ChromeDevToolColorPicker() {
        classCallCheck(this, ChromeDevToolColorPicker);
        return possibleConstructorReturn(this, (ChromeDevToolColorPicker.__proto__ || Object.getPrototypeOf(ChromeDevToolColorPicker)).apply(this, arguments));
    }

    createClass(ChromeDevToolColorPicker, [{
        key: 'template',
        value: function template() {
            return '\n            <div class=\'colorpicker-body\'>\n                <div target="palette"></div> \n                <div target="control"></div>\n                <div target="information"></div>\n                <div target="currentColorSets"></div>\n                <div target="colorSetsChooser"></div>\n                <div target="contextMenu"></div>\n            </div>\n        ';
        }
    }, {
        key: 'components',
        value: function components() {
            return {
                palette: ColorPalette,
                control: ColorControl$2,
                information: ColorInformation,
                currentColorSets: CurrentColorSets,
                colorSetsChooser: ColorSetsChooser,
                contextMenu: CurrentColorSetsContextMenu
            };
        }
    }]);
    return ChromeDevToolColorPicker;
}(BaseColorPicker);

var source$5 = 'mini-control';

var ColorControl$4 = function (_UIElement) {
    inherits(ColorControl, _UIElement);

    function ColorControl() {
        classCallCheck(this, ColorControl);
        return possibleConstructorReturn(this, (ColorControl.__proto__ || Object.getPrototypeOf(ColorControl)).apply(this, arguments));
    }

    createClass(ColorControl, [{
        key: 'components',
        value: function components() {
            return { Hue: Hue, Opacity: Opacity };
        }
    }, {
        key: 'template',
        value: function template() {
            return '\n        <div class="control">\n            <div target="Hue" ></div>\n            <div target="Opacity" ></div>\n        </div>\n        ';
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setColorUI();
        }
    }, {
        key: 'setColorUI',
        value: function setColorUI() {
            this.Hue.setColorUI();
            this.Opacity.setColorUI();
        }
    }, {
        key: '@changeColor',
        value: function changeColor(sourceType) {
            if (source$5 != sourceType) {
                this.refresh();
            }
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }]);
    return ColorControl;
}(UIElement);

var MiniColorPicker = function (_BaseColorPicker) {
    inherits(MiniColorPicker, _BaseColorPicker);

    function MiniColorPicker() {
        classCallCheck(this, MiniColorPicker);
        return possibleConstructorReturn(this, (MiniColorPicker.__proto__ || Object.getPrototypeOf(MiniColorPicker)).apply(this, arguments));
    }

    createClass(MiniColorPicker, [{
        key: 'template',
        value: function template() {
            return '\n            <div class=\'colorpicker-body\'>\n                <div target="palette"></div>\n                <div target="control"></div>\n            </div>\n        ';
        }
    }, {
        key: 'components',
        value: function components() {
            return {
                palette: ColorPalette,
                control: ColorControl$4
            };
        }
    }]);
    return MiniColorPicker;
}(BaseColorPicker);

var VerticalSlider = function (_BaseSlider) {
    inherits(VerticalSlider, _BaseSlider);

    function VerticalSlider(opt) {
        classCallCheck(this, VerticalSlider);

        var _this = possibleConstructorReturn(this, (VerticalSlider.__proto__ || Object.getPrototypeOf(VerticalSlider)).call(this, opt));

        _this.source = 'vertical-slider';
        return _this;
    }

    /** get max height for vertical slider */


    createClass(VerticalSlider, [{
        key: 'getMaxDist',
        value: function getMaxDist() {
            return this.state.get('$container.height');
        }

        /** set mouse pointer for vertical slider */

    }, {
        key: 'setMousePosition',
        value: function setMousePosition(y) {
            this.refs.$bar.css({ top: y + 'px' });
        }

        /** get mouse position by pageY for vertical slider */

    }, {
        key: 'getMousePosition',
        value: function getMousePosition(e) {
            return Event.pos(e).pageY;
        }

        /** get min position for vertial slider */

    }, {
        key: 'getMinPosition',
        value: function getMinPosition() {
            return this.refs.$container.offset().top;
        }

        /** get caculated dist for domain value   */

    }, {
        key: 'getCaculatedDist',
        value: function getCaculatedDist(e) {
            var current = e ? this.getMousePosition(e) : this.getCurrent(this.getDefaultValue() / this.maxValue);
            var dist = 100 - this.getDist(current);

            return dist;
        }

        /** set drag bar position  */

    }, {
        key: 'setColorUI',
        value: function setColorUI(v) {

            v = v || this.getDefaultValue();

            if (v <= this.minValue) {
                this.refs.$bar.addClass('first').removeClass('last');
            } else if (v >= this.maxValue) {
                this.refs.$bar.addClass('last').removeClass('first');
            } else {
                this.refs.$bar.removeClass('last').removeClass('first');
            }

            var per = 1 - (v || 0) / this.maxValue;

            this.setMousePosition(this.getMaxDist() * per);
        }
    }]);
    return VerticalSlider;
}(BaseSlider);

var VerticalHue = function (_VerticalSlider) {
    inherits(VerticalHue, _VerticalSlider);

    function VerticalHue(opt) {
        classCallCheck(this, VerticalHue);

        var _this = possibleConstructorReturn(this, (VerticalHue.__proto__ || Object.getPrototypeOf(VerticalHue)).call(this, opt));

        _this.minValue = 0;
        _this.maxValue = 360;
        _this.source = 'vertical-hue-control';
        return _this;
    }

    createClass(VerticalHue, [{
        key: 'template',
        value: function template() {
            return '\n            <div class="hue">\n                <div ref="$container" class="hue-container">\n                    <div ref="$bar" class="drag-bar"></div>\n                </div>\n            </div>\n        ';
        }
    }, {
        key: 'getDefaultValue',
        value: function getDefaultValue() {
            return this.$store.hsv.h;
        }
    }, {
        key: 'refreshColorUI',
        value: function refreshColorUI(e) {

            var dist = this.getCaculatedDist(e);

            this.setColorUI(dist / 100 * this.maxValue);

            this.changeColor({
                h: dist / 100 * this.maxValue,
                type: 'hsv'
            });
        }
    }]);
    return VerticalHue;
}(VerticalSlider);

var Opacity$2 = function (_VerticalSlider) {
    inherits(Opacity, _VerticalSlider);

    function Opacity(opt) {
        classCallCheck(this, Opacity);

        var _this = possibleConstructorReturn(this, (Opacity.__proto__ || Object.getPrototypeOf(Opacity)).call(this, opt));

        _this.source = 'vertical-opacity-control';
        return _this;
    }

    createClass(Opacity, [{
        key: 'template',
        value: function template() {
            return '\n        <div class="opacity">\n            <div ref="$container" class="opacity-container">\n                <div ref="$colorbar" class="color-bar"></div>\n                <div ref="$bar" class="drag-bar2"></div>\n            </div>\n        </div>\n        ';
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            get(Opacity.prototype.__proto__ || Object.getPrototypeOf(Opacity.prototype), 'refresh', this).call(this);
            this.setOpacityColorBar();
        }
    }, {
        key: 'setOpacityColorBar',
        value: function setOpacityColorBar() {
            var rgb = Object.assign({}, this.$store.rgb);

            rgb.a = 0;
            var start = Color$1.format(rgb, 'rgb');

            rgb.a = 1;
            var end = Color$1.format(rgb, 'rgb');

            this.refs.$colorbar.css('background', 'linear-gradient(to top, ' + start + ', ' + end + ')');
        }
    }, {
        key: 'getDefaultValue',
        value: function getDefaultValue() {
            return this.$store.alpha;
        }
    }, {
        key: 'refreshColorUI',
        value: function refreshColorUI(e) {
            var dist = this.getCaculatedDist(e);

            this.setColorUI(dist / 100 * this.maxValue);

            this.changeColor({
                a: Math.floor(dist) / 100 * this.maxValue
            });
        }
    }]);
    return Opacity;
}(VerticalSlider);

var source$6 = 'mini-control';

var ColorControl$6 = function (_UIElement) {
    inherits(ColorControl, _UIElement);

    function ColorControl() {
        classCallCheck(this, ColorControl);
        return possibleConstructorReturn(this, (ColorControl.__proto__ || Object.getPrototypeOf(ColorControl)).apply(this, arguments));
    }

    createClass(ColorControl, [{
        key: 'components',
        value: function components() {
            return { Hue: VerticalHue, Opacity: Opacity$2 };
        }
    }, {
        key: 'template',
        value: function template() {
            return '<div class="control"><div target="Hue" ></div><div target="Opacity" ></div></div>';
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setColorUI();
        }
    }, {
        key: 'setColorUI',
        value: function setColorUI() {
            this.Hue.setColorUI();
            this.Opacity.setColorUI();
        }
    }, {
        key: '@changeColor',
        value: function changeColor(sourceType) {
            if (source$6 != sourceType) {
                this.refresh();
            }
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }]);
    return ColorControl;
}(UIElement);

var MiniColorPicker$2 = function (_BaseColorPicker) {
    inherits(MiniColorPicker, _BaseColorPicker);

    function MiniColorPicker() {
        classCallCheck(this, MiniColorPicker);
        return possibleConstructorReturn(this, (MiniColorPicker.__proto__ || Object.getPrototypeOf(MiniColorPicker)).apply(this, arguments));
    }

    createClass(MiniColorPicker, [{
        key: 'template',
        value: function template() {
            return '\n            <div class=\'colorpicker-body\'>\n                <div target="palette"></div><div target="control"></div>\n            </div>\n        ';
        }
    }, {
        key: 'components',
        value: function components() {
            return {
                palette: ColorPalette,
                control: ColorControl$6
            };
        }
    }]);
    return MiniColorPicker;
}(BaseColorPicker);

var source$7 = 'macos-control';

var ColorControl$8 = function (_UIElement) {
    inherits(ColorControl, _UIElement);

    function ColorControl() {
        classCallCheck(this, ColorControl);
        return possibleConstructorReturn(this, (ColorControl.__proto__ || Object.getPrototypeOf(ColorControl)).apply(this, arguments));
    }

    createClass(ColorControl, [{
        key: 'components',
        value: function components() {
            return { Value: Value, Opacity: Opacity };
        }
    }, {
        key: 'template',
        value: function template() {
            return '\n        <div class="control">\n            <div target="Value" ></div>\n            <div target="Opacity" ></div>\n            <div ref="$controlPattern" class="empty"></div>\n            <div ref="$controlColor" class="color"></div>\n        </div>\n        ';
        }
    }, {
        key: 'setBackgroundColor',
        value: function setBackgroundColor() {
            this.refs.$controlColor.css("background-color", this.$store.dispatch('/toRGB'));
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setColorUI();
            this.setBackgroundColor();
        }
    }, {
        key: 'setColorUI',
        value: function setColorUI() {
            this.Value.setColorUI();
            this.Opacity.setColorUI();
        }
    }, {
        key: '@changeColor',
        value: function changeColor(sourceType) {
            if (source$7 != sourceType) {
                this.refresh();
            }
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }]);
    return ColorControl;
}(UIElement);

var ColorRing = function (_ColorWheel) {
    inherits(ColorRing, _ColorWheel);

    function ColorRing(opt) {
        classCallCheck(this, ColorRing);

        var _this = possibleConstructorReturn(this, (ColorRing.__proto__ || Object.getPrototypeOf(ColorRing)).call(this, opt));

        _this.width = 214;
        _this.height = 214;
        _this.thinkness = 16;
        _this.half_thinkness = _this.thinkness / 2;
        _this.source = 'colorring';
        return _this;
    }

    createClass(ColorRing, [{
        key: 'template',
        value: function template() {
            return '\n        <div class="wheel" data-type="ring">\n            <canvas class="wheel-canvas" ref="$colorwheel" ></canvas>\n            <div class="drag-pointer" ref="$drag_pointer"></div>\n        </div>\n        ';
        }
    }, {
        key: 'setColorUI',
        value: function setColorUI(isEvent) {
            this.renderCanvas();
            this.setHueColor(null, isEvent);
        }
    }, {
        key: 'getDefaultValue',
        value: function getDefaultValue() {
            return this.$store.hsv.h;
        }
    }, {
        key: 'setHueColor',
        value: function setHueColor(e, isEvent) {

            if (!this.state.get('$el.width')) return;

            var _getRectangle = this.getRectangle(),
                minX = _getRectangle.minX,
                minY = _getRectangle.minY,
                radius = _getRectangle.radius,
                centerX = _getRectangle.centerX,
                centerY = _getRectangle.centerY;

            var _getCurrentXY = this.getCurrentXY(e, this.getDefaultValue(), radius, centerX, centerY),
                x = _getCurrentXY.x,
                y = _getCurrentXY.y;

            var rx = x - centerX,
                ry = y - centerY,
                hue = caculateAngle(rx, ry);

            {
                var _getCurrentXY2 = this.getCurrentXY(null, hue, radius - this.half_thinkness, centerX, centerY),
                    x = _getCurrentXY2.x,
                    y = _getCurrentXY2.y;
            }

            // set drag pointer position 
            this.refs.$drag_pointer.css({
                left: x - minX + 'px',
                top: y - minY + 'px'
            });

            if (!isEvent) {
                this.changeColor({
                    type: 'hsv',
                    h: hue
                });
            }
        }
    }]);
    return ColorRing;
}(ColorWheel);

// import ColorWheel from '../ui/ColorWheel'
var RingColorPicker = function (_BaseColorPicker) {
    inherits(RingColorPicker, _BaseColorPicker);

    function RingColorPicker() {
        classCallCheck(this, RingColorPicker);
        return possibleConstructorReturn(this, (RingColorPicker.__proto__ || Object.getPrototypeOf(RingColorPicker)).apply(this, arguments));
    }

    createClass(RingColorPicker, [{
        key: 'template',
        value: function template() {
            return '\n            <div class=\'colorpicker-body\'>\n                <div target="colorring"></div>\n                <div target="palette"></div> \n                <div target="control"></div>\n                <div target="information"></div>\n                <div target="currentColorSets"></div>\n                <div target="colorSetsChooser"></div>\n                <div target="contextMenu"></div>\n            </div>\n        ';
        }
    }, {
        key: 'components',
        value: function components() {
            return {
                colorring: ColorRing,
                palette: ColorPalette,
                control: ColorControl$8,
                information: ColorInformation,
                currentColorSets: CurrentColorSets,
                colorSetsChooser: ColorSetsChooser,
                contextMenu: CurrentColorSetsContextMenu
            };
        }
    }]);
    return RingColorPicker;
}(BaseColorPicker);

var ColorControl$10 = function (_UIElement) {
    inherits(ColorControl, _UIElement);

    function ColorControl() {
        classCallCheck(this, ColorControl);
        return possibleConstructorReturn(this, (ColorControl.__proto__ || Object.getPrototypeOf(ColorControl)).apply(this, arguments));
    }

    createClass(ColorControl, [{
        key: 'components',
        value: function components() {
            return { Hue: VerticalHue, Opacity: Opacity$2 };
        }
    }, {
        key: 'template',
        value: function template() {
            return '\n        <div class="control">\n            <div target="Hue" ></div>\n            <div target="Opacity" ></div>\n        </div>\n        ';
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setColorUI();
        }
    }, {
        key: 'setColorUI',
        value: function setColorUI() {
            this.Hue.setColorUI();
            this.Opacity.setColorUI();
        }
    }, {
        key: '@changeColor',
        value: function changeColor() {
            this.refresh();
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }]);
    return ColorControl;
}(UIElement);

var XDColorPicker = function (_BaseColorPicker) {
    inherits(XDColorPicker, _BaseColorPicker);

    function XDColorPicker() {
        classCallCheck(this, XDColorPicker);
        return possibleConstructorReturn(this, (XDColorPicker.__proto__ || Object.getPrototypeOf(XDColorPicker)).apply(this, arguments));
    }

    createClass(XDColorPicker, [{
        key: 'template',
        value: function template() {
            return '\n            <div class=\'colorpicker-body\'>\n                <div target="palette"></div> \n                <div target="control"></div>\n                <div target="information"></div>\n                <div target="currentColorSets"></div>\n                <div target="colorSetsChooser"></div>\n                <div target="contextMenu"></div>\n            </div>\n        ';
        }
    }, {
        key: 'components',
        value: function components() {
            return {
                palette: ColorPalette,
                control: ColorControl$10,
                information: ColorInformation,
                currentColorSets: CurrentColorSets,
                colorSetsChooser: ColorSetsChooser,
                contextMenu: CurrentColorSetsContextMenu
            };
        }
    }]);
    return XDColorPicker;
}(BaseColorPicker);

var source$8 = 'mini-control';

var ColorControl$12 = function (_UIElement) {
    inherits(ColorControl, _UIElement);

    function ColorControl() {
        classCallCheck(this, ColorControl);
        return possibleConstructorReturn(this, (ColorControl.__proto__ || Object.getPrototypeOf(ColorControl)).apply(this, arguments));
    }

    createClass(ColorControl, [{
        key: 'components',
        value: function components() {
            return { Hue: VerticalHue, Opacity: Opacity$2 };
        }
    }, {
        key: 'template',
        value: function template() {
            return (/*html*/'\n            <div class="control">\n                <div target="Opacity" ></div>            \n                <div target="Hue" ></div>\n            </div>\n        '
            );
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setColorUI();
        }
    }, {
        key: 'setColorUI',
        value: function setColorUI() {
            this.Hue.setColorUI();
            this.Opacity.setColorUI();
        }
    }, {
        key: '@changeColor',
        value: function changeColor(sourceType) {
            if (source$8 != sourceType) {
                this.refresh();
            }
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }]);
    return ColorControl;
}(UIElement);

var VSCodePicker = function (_BaseColorPicker) {
    inherits(VSCodePicker, _BaseColorPicker);

    function VSCodePicker() {
        classCallCheck(this, VSCodePicker);
        return possibleConstructorReturn(this, (VSCodePicker.__proto__ || Object.getPrototypeOf(VSCodePicker)).apply(this, arguments));
    }

    createClass(VSCodePicker, [{
        key: 'template',
        value: function template() {
            return (/*html*/'\n            <div class=\'colorpicker-body\'>\n                <div class=\'color-view\'>\n                    <div class=\'color-view-container\'  ref="$colorview"></div>\n                </div>\n                <div class=\'color-tool\'>\n                    <div target="palette"></div>\n                    <div target="control"></div>\n                </div>\n            </div>\n        '
            );
        }
    }, {
        key: 'components',
        value: function components() {
            return {
                palette: ColorPalette,
                control: ColorControl$12
            };
        }
    }, {
        key: 'initColorWithoutChangeEvent',
        value: function initColorWithoutChangeEvent(color) {
            this.$store.dispatch('/initColor', color);
            this.refresh();
        }
    }, {
        key: 'setBackgroundColor',
        value: function setBackgroundColor() {
            var color = this.$store.dispatch('/toColor');
            var rgb = this.$store.rgb;
            var bValue = Color$1.brightness(rgb.r, rgb.g, rgb.b);

            this.refs.$colorview.css({
                "background-color": color,
                'color': bValue > 127 ? 'black' : 'white'
            });
            this.refs.$colorview.html(color);
        }
    }, {
        key: 'click $colorview',
        value: function click$colorview(e) {
            this.nextFormat();
        }
    }, {
        key: 'nextFormat',
        value: function nextFormat() {
            var current_format = this.$store.format || 'hex';

            var next_format = 'hex';
            if (current_format == 'hex') {
                next_format = 'rgb';
            } else if (current_format == 'rgb') {
                next_format = 'hsl';
            } else if (current_format == 'hsl') {
                next_format = 'hex';
            }

            this.$store.dispatch('/changeFormat', next_format);
            this.$store.emit('lastUpdateColor');
            this.refresh();
        }
    }, {
        key: 'refresh',
        value: function refresh() {
            this.setBackgroundColor();
        }
    }, {
        key: '@changeColor',
        value: function changeColor() {
            this.refresh();
        }
    }, {
        key: '@initColor',
        value: function initColor() {
            this.refresh();
        }
    }]);
    return VSCodePicker;
}(BaseColorPicker);

var ColorPicker = {
    create: function create(opts) {
        switch (opts.type) {
            case 'macos':
                return new MacOSColorPicker(opts);
            case 'xd':
                return new XDColorPicker(opts);
            case 'ring':
                return new RingColorPicker(opts);
            case 'mini':
                return new MiniColorPicker(opts);
            case 'vscode':
                return new VSCodePicker(opts);
            case 'mini-vertical':
                return new MiniColorPicker$2(opts);
            case 'sketch':
            case 'palette':
            default:
                return new ChromeDevToolColorPicker(opts);
        }
    },

    ColorPicker: ChromeDevToolColorPicker,
    ChromeDevToolColorPicker: ChromeDevToolColorPicker,
    MacOSColorPicker: MacOSColorPicker,
    RingColorPicker: RingColorPicker,
    MiniColorPicker: MiniColorPicker,
    VSCodePicker: VSCodePicker,
    MiniVerticalColorPicker: MiniColorPicker$2
};

var colorpicker_class = 'codemirror-colorview';
var colorpicker_background_class = 'codemirror-colorview-background';
// Excluded tokens do not show color views..
var excluded_token = ['comment', 'builtin', 'qualifier'];

function onChange(cm, evt) {
    if (evt.origin == 'setValue') {
        // if content is changed by setValue method, it initialize markers
        // cm.state.colorpicker.close_color_picker();
        cm.state.colorpicker.init_color_update();
        cm.state.colorpicker.style_color_update();
    } else {
        cm.state.colorpicker.style_color_update(cm.getCursor().line);
    }
}

function onUpdate(cm, evt) {
    if (!cm.state.colorpicker.isUpdate) {
        cm.state.colorpicker.isUpdate = true;
        cm.state.colorpicker.close_color_picker();
        cm.state.colorpicker.init_color_update();
        cm.state.colorpicker.style_color_update();
    }
}

function onRefresh(cm, evt) {
    onChange(cm, { origin: 'setValue' });
}

function onKeyup(cm, evt) {
    cm.state.colorpicker.keyup(evt);
}

function onMousedown(cm, evt) {
    if (cm.state.colorpicker.is_edit_mode()) {
        cm.state.colorpicker.check_mousedown(evt);
    }
}

function onPaste(cm, evt) {
    onChange(cm, { origin: 'setValue' });
}

function onScroll(cm) {
    cm.state.colorpicker.close_color_picker();
}

function onBlur(cm) {
    cm.state.colorpicker.hide_delay_color_picker(cm.state.colorpicker.opt.hideDelay || 1000);
}

function debounce(callback, delay) {

    var t = undefined;

    return function (cm, e) {
        if (t) {
            clearTimeout(t);
        }

        t = setTimeout(function () {
            callback(cm, e);
        }, delay || 300);
    };
}

function has_class(el, cls) {
    if (!el || !el.className) {
        return false;
    } else {
        var newClass = ' ' + el.className + ' ';
        return newClass.indexOf(' ' + cls + ' ') > -1;
    }
}

var ColorView = function () {
    function ColorView(cm, opt) {
        classCallCheck(this, ColorView);

        if (typeof opt == 'boolean') {
            opt = { mode: 'edit' };
        } else {
            opt = Object.assign({ mode: 'edit' }, opt || {});
        }

        this.opt = opt;
        this.cm = cm;
        this.markers = {};

        // set excluded token 
        this.excluded_token = this.opt.excluded_token || excluded_token;

        if (this.opt.colorpicker) {
            this.colorpicker = this.opt.colorpicker(this.opt);
        } else {
            this.colorpicker = ColorPicker.create(this.opt);
        }

        this.init_event();
    }

    createClass(ColorView, [{
        key: 'init_event',
        value: function init_event() {

            this.cm.on('mousedown', onMousedown);
            this.cm.on('keyup', onKeyup);
            this.cm.on('change', onChange);
            this.cm.on('update', onUpdate);
            this.cm.on('refresh', onRefresh);
            this.cm.on('blur', onBlur);

            // create paste callback
            this.onPasteCallback = function (cm, callback) {
                return function (evt) {
                    callback.call(this, cm, evt);
                };
            }(this.cm, onPaste);

            this.onScrollEvent = debounce(onScroll, 50);

            this.cm.getWrapperElement().addEventListener('paste', this.onPasteCallback);

            if (this.is_edit_mode()) {
                this.cm.on('scroll', this.onScrollEvent);
            }
        }
    }, {
        key: 'is_edit_mode',
        value: function is_edit_mode() {
            return this.opt.mode == 'edit';
        }
    }, {
        key: 'is_view_mode',
        value: function is_view_mode() {
            return this.opt.mode == 'view';
        }
    }, {
        key: 'destroy',
        value: function destroy() {
            this.cm.off('mousedown', onMousedown);
            this.cm.off('keyup', onKeyup);
            this.cm.off('change', onChange);
            this.cm.off('blur', onBlur);

            this.cm.getWrapperElement().removeEventListener('paste', this.onPasteCallback);

            if (this.is_edit_mode()) {
                this.cm.off('scroll', this.onScrollEvent);
            }
        }
    }, {
        key: 'hasClass',
        value: function hasClass(el, className) {
            if (!el.className) {
                return false;
            } else {
                var newClass = ' ' + el.className + ' ';
                return newClass.indexOf(' ' + className + ' ') > -1;
            }
        }
    }, {
        key: 'check_mousedown',
        value: function check_mousedown(evt) {
            if (this.hasClass(evt.target, colorpicker_background_class)) {
                this.open_color_picker(evt.target.parentNode);
            } else {
                this.close_color_picker();
            }
        }
    }, {
        key: 'popup_color_picker',
        value: function popup_color_picker(defalutColor) {
            var cursor = this.cm.getCursor();
            var self = this;
            var colorMarker = {
                lineNo: cursor.line,
                ch: cursor.ch,
                color: defalutColor || '#FFFFFF',
                isShortCut: true
            };

            Object.keys(this.markers).forEach(function (key) {
                var searchKey = "#" + key;
                if (searchKey.indexOf("#" + colorMarker.lineNo + ":") > -1) {
                    var marker = self.markers[key];

                    if (marker.ch <= colorMarker.ch && colorMarker.ch <= marker.ch + marker.color.length) {
                        // when cursor has marker
                        colorMarker.ch = marker.ch;
                        colorMarker.color = marker.color;
                        colorMarker.nameColor = marker.nameColor;
                    }
                }
            });

            this.open_color_picker(colorMarker);
        }
    }, {
        key: 'open_color_picker',
        value: function open_color_picker(el) {
            var _this = this;

            var lineNo = el.lineNo;
            var ch = el.ch;
            var nameColor = el.nameColor;
            var color = el.color;

            if (this.colorpicker) {
                var prevColor = color;
                var pos = this.cm.charCoords({ line: lineNo, ch: ch });
                this.colorpicker.show({
                    left: pos.left,
                    top: pos.bottom,
                    isShortCut: el.isShortCut || false,
                    hideDelay: this.opt.hideDelay || 2000
                }, nameColor || color, function (newColor) {
                    _this.cm.replaceRange(newColor, { line: lineNo, ch: ch }, { line: lineNo, ch: ch + prevColor.length }, '*colorpicker');
                    _this.cm.focus();
                    prevColor = newColor;
                });
            }
        }
    }, {
        key: 'close_color_picker',
        value: function close_color_picker() {
            if (this.colorpicker) {
                this.colorpicker.hide();
            }
        }
    }, {
        key: 'hide_delay_color_picker',
        value: function hide_delay_color_picker() {
            if (this.colorpicker) {
                this.colorpicker.runHideDelay();
            }
        }
    }, {
        key: 'key',
        value: function key(lineNo, ch) {
            return [lineNo, ch].join(":");
        }
    }, {
        key: 'keyup',
        value: function keyup(evt) {

            if (this.colorpicker) {
                if (evt.key == 'Escape') {
                    this.colorpicker.hide();
                } else if (this.colorpicker.isShortCut == false) {
                    this.colorpicker.hide();
                }
            }
        }
    }, {
        key: 'init_color_update',
        value: function init_color_update() {
            this.markers = {}; // initialize marker list
        }
    }, {
        key: 'style_color_update',
        value: function style_color_update(lineHandle) {
            if (lineHandle) {
                this.match(lineHandle);
            } else {
                var max = this.cm.lineCount();

                for (var lineNo = 0; lineNo < max; lineNo++) {
                    this.match(lineNo);
                }
            }
        }
    }, {
        key: 'empty_marker',
        value: function empty_marker(lineNo, lineHandle) {
            var list = lineHandle.markedSpans || [];

            for (var i = 0, len = list.length; i < len; i++) {
                var key = this.key(lineNo, list[i].from);

                if (key && has_class(list[i].marker.replacedWith, colorpicker_class)) {
                    delete this.markers[key];
                    list[i].marker.clear();
                }
            }
        }
    }, {
        key: 'match_result',
        value: function match_result(lineHandle) {
            return Color$1.matches(lineHandle.text);
        }
    }, {
        key: 'submatch',
        value: function submatch(lineNo, lineHandle) {
            var _this2 = this;

            this.empty_marker(lineNo, lineHandle);

            var result = this.match_result(lineHandle);
            var obj = { next: 0 };

            result.forEach(function (item) {
                _this2.render(obj, lineNo, lineHandle, item.color, item.nameColor);
            });
        }
    }, {
        key: 'match',
        value: function match(lineNo) {
            var lineHandle = this.cm.getLineHandle(lineNo);
            var self = this;
            this.cm.operation(function () {
                self.submatch(lineNo, lineHandle);
            });
        }
    }, {
        key: 'make_element',
        value: function make_element() {
            var el = document.createElement('div');

            el.className = colorpicker_class;

            if (this.is_edit_mode()) {
                el.title = "open color picker";
            } else {
                el.title = "";
            }

            el.back_element = this.make_background_element();
            el.appendChild(el.back_element);

            return el;
        }
    }, {
        key: 'make_background_element',
        value: function make_background_element() {
            var el = document.createElement('div');

            el.className = colorpicker_background_class;

            return el;
        }
    }, {
        key: 'set_state',
        value: function set_state(lineNo, start, color, nameColor) {
            var marker = this.create_marker(lineNo, start);

            marker.lineNo = lineNo;
            marker.ch = start;
            marker.color = color;
            marker.nameColor = nameColor;

            return marker;
        }
    }, {
        key: 'create_marker',
        value: function create_marker(lineNo, start) {

            if (!this.has_marker(lineNo, start)) {
                this.init_marker(lineNo, start);
            }

            return this.get_marker(lineNo, start);
        }
    }, {
        key: 'init_marker',
        value: function init_marker(lineNo, start) {
            this.markers[this.key(lineNo, start)] = this.make_element();
        }
    }, {
        key: 'has_marker',
        value: function has_marker(lineNo, start) {
            return !!this.get_marker(lineNo, start);
        }
    }, {
        key: 'get_marker',
        value: function get_marker(lineNo, start) {
            var key = this.key(lineNo, start);
            return this.markers[key];
        }
    }, {
        key: 'update_element',
        value: function update_element(el, color) {
            el.back_element.style.backgroundColor = color;
        }
    }, {
        key: 'set_mark',
        value: function set_mark(line, ch, el) {
            this.cm.setBookmark({ line: line, ch: ch }, { widget: el, handleMouseEvents: true });
        }
    }, {
        key: 'is_excluded_token',
        value: function is_excluded_token(line, ch) {
            var token = this.cm.getTokenAt({ line: line, ch: ch }, true);
            var type = token.type;
            var state = token.state.state;

            if (type == null && state == 'block') return true;
            if (type == null && state == 'top') return true;
            // if (type == null && state == 'prop')  return true;

            var count = 0;
            for (var i = 0, len = this.excluded_token.length; i < len; i++) {
                if (type === this.excluded_token[i]) {
                    count++;
                    break;
                }
            }

            return count > 0; // true is that it has a excluded token 
        }
    }, {
        key: 'render',
        value: function render(cursor, lineNo, lineHandle, color, nameColor) {
            var start = lineHandle.text.indexOf(color, cursor.next);

            if (this.is_excluded_token(lineNo, start) === true) {
                // excluded token do not show.
                return;
            }

            cursor.next = start + color.length;

            if (this.has_marker(lineNo, start)) {
                this.update_element(this.create_marker(lineNo, start), nameColor || color);
                this.set_state(lineNo, start, color, nameColor);
                return;
            }

            var el = this.create_marker(lineNo, start);

            this.update_element(el, nameColor || color);

            this.set_state(lineNo, start, color, nameColor || color);
            this.set_mark(lineNo, start, el);
        }
    }]);
    return ColorView;
}();

try {
    var CodeMirror = require('codemirror');
} catch (e) {}

var CHECK_CODEMIRROR_OBJECT = function CHECK_CODEMIRROR_OBJECT() {
    return CodeMirror || window.CodeMirror;
};
function LOAD_CODEMIRROR_COLORPICKER() {
    var CODEMIRROR_OBJECT = CHECK_CODEMIRROR_OBJECT();

    if (CODEMIRROR_OBJECT) {
        CODEMIRROR_OBJECT.defineOption("colorpicker", false, function (cm, val, old) {
            if (old && old != CODEMIRROR_OBJECT.Init) {

                if (cm.state.colorpicker) {
                    cm.state.colorpicker.destroy();
                    cm.state.colorpicker = null;
                }
                // remove event listener
            }

            if (val) {
                cm.state.colorpicker = new ColorView(cm, val);
            }
        });
    }
}

LOAD_CODEMIRROR_COLORPICKER();

var CodeMirrorExtension = {
    load: LOAD_CODEMIRROR_COLORPICKER
};

var index = _extends({}, Util, ColorPicker, CodeMirrorExtension);

return index;

})));
window["codemirror-picker"] = self["codemirror-picker"];
