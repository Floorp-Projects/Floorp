/**
 * jQuery Lint
 * ---
 * VERSION 0.36
 * ---
 * jQuery lint creates a thin blanket over jQuery that'll
 * report any potentially erroneous activity the console.
 * ---
 * Idea from:
 *      http://markmail.org/message/wzkosk2s5jklpkv4
 *      http://groups.google.com/group/jquery-dev/browse_thread/thread/9a15cca62ceb2444
 * ---
 * @author James Padolsey
 * @contributors Paul Irish
 * ---
 * Dual licensed under the MIT and GPL licenses.
 *    - http://www.opensource.org/licenses/mit-license.php
 *    - http://www.gnu.org/copyleft/gpl.html
 */

(function(){
    
    var alias = 'jQuery',
    
        glob = this,
        
        // Define console if not defined
        // Access it via jQuery.LINT.console
        _console = {
            
            warn: glob.console && console.warn ?
                function(){console.warn.apply(console, arguments);} : function(){},
                
            group: glob.console && console.group ?
                function(){console.group.apply(console, arguments);} : function(){},
                
            groupEnd: glob.console && console.groupEnd ?
                function(){console.groupEnd();} : function(){},
                
            groupCollapsed: glob.console && console.groupCollapsed ?
                function(){console.groupCollapsed.apply(console, arguments);} : function(){},
                
            log: glob.console && console.log ?
                function(){console.log.apply(console, arguments);} : function(){}
                
        },
        
        langs = {
            en: {
                incorrectCall: '%0(...) called incorrectly',
                specialCheckFailed: '%0(...) special check failed',
                moreInfo: 'More info:',
                youPassed: 'You passed: ',
                collection: 'Collection:',
                availableSigsInclude: 'Available signatures include: ',
                errorThrown: 'When I called %0(...) with your args, an error was thrown!',
                repeatSelector: "You've used the same selector more than once.",
                info: 'Info',
                selector: 'Selector: ',
                selectorAdvice: "You should only use the same selector more than once when you know the returned collection will be different. For example, if you've added more elements to the page that may comply with the selector",
                noElementsFound: 'No elements were found with the selector: "%0"',
                combineCalls: 'Why not combine these calls by passing an object? E.g. \n%0(%1)',
                methodTwice: "You've called %0(...) more than once on the same jQuery object",
                triggeredBy: 'Triggered by %0 event',
                event: 'Event:',
                handler: 'Handler:',
                location: 'Location:',
                invalidFilters: 'Selector: %0\nYou used invalid filters (aka Pseudo classes):\n%1',
                badReadyCall: "Don't use jQuery().ready() - use jQuery(document).ready() instead. The former is likely to be deprecated in the future.",
                browser: "Don't use jQuery.browser",
                browserSafari: "Don't use jQuery.browser.safari - it's deprecated. If you have to use browser detection, then use jQuery.browser.webkit.",
                featureDetection: 'The jQuery team recommends against using jQuery.browser, please try to use feature detection instead (see jQuery.support).',
                boxModel: "Don't use jQuery.boxModel.",
                boxModelDeprecated: 'Deprecated in jQuery 1.3 (see jQuery.support)'
            }
        },
        
        // Add specific checks
        // This is the best place to bring up bad practices
        specialChecks = [
            {/* Level 0 */},
            {/* Level 1 */},
            {/* Level 2 */},
            {/* Level 3 */}
        ],
        
        // Local scope jQuery
        _jQuery = glob[alias],
        
        lint = {
            version: '0.36',
            level: 3,
            special: specialChecks,
            lang: 'en',
            langs: langs,
            console: _console,
            'throw': false,
            specific: {
                // True to report, false to supress
                noElementsFound: true,
                repeatSelector: true,
                browserSniffing: true,
                invalidFilters: true
            }
        },
        
        // Only cover certain fns under the jQ namespace
        jQNameSpace = /^(getJSON|extend|ajax|get|post|proxy|each|map|queue|ajax.+|removeData|data|pushStack)$/,
        
        // API data, only with what we need
        api = {"jQuery.proxy":[{added:"1.4",arg:[{name:"function",type:"Function"},{name:"scope",type:"Object"}]},{added:"1.4",arg:[{name:"scope",type:"Object"},{name:"name",type:"String"}]}],focusout:[{added:"1.4",arg:[{name:"handler(eventObject)",type:"Function"}]}],focusin:[{added:"1.4",arg:[{name:"handler(eventObject)",type:"Function"}]}],has:[{added:"1.4",arg:[{name:"selector",type:"String"}]},{added:"1.4",arg:[{name:"contained",type:"Element"}]},{added:"1.1.4"}],"jQuery.contains":[{added:"1.4",arg:[{name:"container",type:"Element"},{name:"contained",type:"Element"}]}],"jQuery.noop":[{added:"1.4"}],delay:[{added:"1.4",arg:[{name:"duration",type:"Integer"},{name:"queueName",type:"String",optional:true}]}],parentsUntil:[{added:"1.4",arg:[{name:"selector",type:"Selector",optional:true}]}],prevUntil:[{added:"1.4",arg:[{name:"selector",type:"Selector",optional:true}]}],nextUntil:[{added:"1.4",arg:[{name:"selector",type:"Selector",optional:true}]}],"event.isImmediatePropagationStopped":[{added:"1.3"}],"event.stopImmediatePropagation":[{added:"1.3"}],"event.isPropagationStopped":[{added:"1.3"}],"event.stopPropagation":[{added:"1.0"}],"event.isDefaultPrevented":[{added:"1.3"}],"event.preventDefault":[{added:"1.0"}],"event.timeStamp":[{added:"1.2.6"}],"event.result":[{added:"1.3"}],"event.which":[{added:"1.1.3"}],"event.pageY":[{added:"1.0.4"}],"event.pageX":[{added:"1.0.4"}],"event.currentTarget":[{added:"1.3"}],"event.relatedTarget":[{added:"1.1.4"}],"event.data":[{added:"1.1"}],"event.target":[{added:"1.0"}],"event.type":[{added:"1.0"}],"jQuery.fx.off":[{added:"1.3"}],each:[{added:"1.0",arg:[{name:"function(index, Element)",type:"Function"}]}],"jQuery.pushStack":[{added:"1.0",arg:[{name:"elements",type:"Array"}]},{added:"1.3",arg:[{name:"elements",type:"Array"},{name:"name",type:"String"},{name:"arguments",type:"Array"}]}],"jQuery.globalEval":[{added:"1.0.4",arg:[{name:"code",type:"String"}]}],"jQuery.isXMLDoc":[{added:"1.1.4",arg:[{name:"node",type:"Element"}]}],"jQuery.removeData":[{added:"1.2.3",arg:[{name:"name",type:"String",optional:true}]}],"jQuery.data":[{added:"1.2.3",arg:[{name:"element",type:"Element"},{name:"key",type:"String"},{name:"value",type:"Object"}]},{added:"1.2.3",arg:[{name:"element",type:"Element"},{name:"key",type:"String"}]},{added:"1.4"}],"jQuery.dequeue":[{added:"1.3",arg:[{name:"queueName",type:"String",optional:true}]}],"jQuery.queue":[{added:"1.3",arg:[{name:"queueName",type:"String",optional:true}]},{added:"1.3",arg:[{name:"queueName",type:"String",optional:true},{name:"newQueue",type:"Array"}]},{added:"1.3",arg:[{name:"queueName",type:"String",optional:true},{name:"callback()",type:"Function"}]}],clearQueue:[{added:"1.4",arg:[{name:"queueName",type:"String",optional:true}]}],toArray:[{added:"1.4"}],"jQuery.isEmptyObject":[{added:"1.4",arg:[{name:"object",type:"Object"}]}],"jQuery.isPlainObject":[{added:"1.4",arg:[{name:"object",type:"Object"}]}],keydown:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],index:[{added:"1.4"},{added:"1.4",arg:[{name:"selector",type:"Selector"}]},{added:"1.0",arg:[{name:"element",type:"Element, jQuery"}]}],removeData:[{added:"1.2.3",arg:[{name:"name",type:"String",optional:true}]}],data:[{added:"1.2.3",arg:[{name:"key",type:"String"},{name:"value",type:"Object"}]},{added:"1.4",arg:[{name:"obj",type:"Object"}]},{added:"1.2.3",arg:[{name:"key",type:"String"}]},{added:"1.4"}],get:[{added:"1.0",arg:[{name:"index",type:"Number",optional:true}]}],size:[{added:"1.0"}],"jQuery.noConflict":[{added:"1.0",arg:[{name:"removeAll",type:"Boolean",optional:true}]}],selected:[{added:"1.0"}],checked:[{added:"1.0"}],disabled:[{added:"1.0"}],enabled:[{added:"1.0"}],file:[{added:"1.0"}],button:[{added:"1.0"}],reset:[{added:"1.0"}],image:[{added:"1.0"}],submit:[{added:"1.0"},{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],checkbox:[{added:"1.0"}],radio:[{added:"1.0"}],password:[{added:"1.0"}],text:[{added:"1.0"},{added:"1.0"},{added:"1.0",arg:[{name:"textString",type:"String"}]},{added:"1.4",arg:[{name:"function(index, text)",type:"Function"}]}],input:[{added:"1.0"}],"only-child":[{added:"1.1.4"}],"last-child":[{added:"1.1.4"}],"first-child":[{added:"1.1.4"}],"nth-child":[{added:"1.1.4",arg:[{name:"index",type:"Number/String"}]}],attributeContainsPrefix:[{added:"1.0",arg:[{name:"attribute",type:"String"},{name:"value",type:"String"}]}],attributeContainsWord:[{added:"1.0",arg:[{name:"attribute",type:"String"},{name:"value",type:"String"}]}],attributeMultiple:[{added:"1.0",arg:[{name:"attributeFilter1",type:"Selector"},{name:"attributeFilter2",type:"Selector"},{name:"attributeFilterN",type:"Selector",optional:true}]}],attributeContains:[{added:"1.0",arg:[{name:"attribute",type:"String"},{name:"value",type:"String"}]}],attributeEndsWith:[{added:"1.0",arg:[{name:"attribute",type:"String"},{name:"value",type:"String"}]}],attributeStartsWith:[{added:"1.0",arg:[{name:"attribute",type:"String"},{name:"value",type:"String"}]}],attributeNotEqual:[{added:"1.0",arg:[{name:"attribute",type:"String"},{name:"value",type:"String"}]}],attributeEquals:[{added:"1.0",arg:[{name:"attribute",type:"String"},{name:"value",type:"String"}]}],attributeHas:[{added:"1.0",arg:[{name:"attribute",type:"String"}]}],visible:[{added:"1.0"}],hidden:[{added:"1.0"}],parent:[{added:"1.0"},{added:"1.0",arg:[{name:"selector",type:"Selector",optional:true}]}],empty:[{added:"1.0"},{added:"1.0"}],contains:[{added:"1.1.4"}],animated:[{added:"1.2"}],header:[{added:"1.2"}],lt:[{added:"1.0",arg:[{name:"index",type:"Number"}]}],gt:[{added:"1.0"}],eq:[{added:"1.0",arg:[{name:"index",type:"Number"}]},{added:"1.1.2",arg:[{name:"index",type:"Integer"}]}],odd:[{added:"1.0"}],even:[{added:"1.0"}],not:[{added:"1.0",arg:[{name:"selector",type:"Selector"}]},{added:"1.0",arg:[{name:"selector",type:"Selector"}]},{added:"1.0",arg:[{name:"elements",type:"Elements"}]},{added:"1.0",arg:[{name:"function(index)",type:"Function"}]}],last:[{added:"1.0"},{added:"1.2"}],first:[{added:"1.0"},{added:"1.2"}],"next siblings":[{added:"1.0",arg:[{name:"prev",type:"Selector"},{name:"siblings",type:"Selector"}]}],"next adjacent":[{added:"1.0",arg:[{name:"prev",type:"Selector"},{name:"next",type:"Selector"}]}],child:[{added:"1.0",arg:[{name:"parent",type:"Selector"},{name:"child",type:"Selector"}]}],descendant:[{added:"1.0",arg:[{name:"ancestor",type:"Selector"},{name:"descendant",type:"Selector"}]}],multiple:[{added:"1.0",arg:[{name:"selector1",type:"Selector"},{name:"selector2",type:"Selector"},{name:"selectorN",type:"Selector",optional:true}]}],all:[{added:"1.0"}],"class":[{added:"1.0",arg:[{name:"class",type:"String"}]}],element:[{added:"1.0",arg:[{name:"element",type:"String"}]}],id:[{added:"1.0",arg:[{name:"id",type:"String"}]}],scroll:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],resize:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],dequeue:[{added:"1.2",arg:[{name:"queueName",type:"String",optional:true}]}],queue:[{added:"1.2",arg:[{name:"queueName",type:"String",optional:true}]},{added:"1.2",arg:[{name:"queueName",type:"String",optional:true},{name:"newQueue",type:"Array"}]},{added:"1.2",arg:[{name:"queueName",type:"String",optional:true},{name:"callback( next )",type:"Function"}]}],keyup:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],keypress:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],select:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],change:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],blur:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],focus:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],mousemove:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],hover:[{added:"1.0",arg:[{name:"handlerIn(eventObject)",type:"Function"},{name:"handlerOut(eventObject)",type:"Function"}]}],mouseleave:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],mouseenter:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],mouseout:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],mouseover:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],dblclick:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],click:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],mouseup:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],mousedown:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0"}],error:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]}],unload:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]}],load:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"}]},{added:"1.0",arg:[{name:"url",type:"String"},{name:"data",type:"Map, String",optional:true},{name:"success(responseText, textStatus, XMLHttpRequest)",type:"Function",optional:true}]}],ready:[{added:"1.0",arg:[{name:"handler",type:"Function"}]}],die:[{added:"1.3",arg:[{name:"eventType",type:"String"},{name:"handler",type:"String",optional:true}]}],"jQuery.browser":[{added:"1.0"}],"jQuery.browser.version":[{added:"1.1.3"}],live:[{added:"1.3",arg:[{name:"eventType",type:"String"},{name:"handler",type:"Function"}]},{added:"1.4",arg:[{name:"eventType",type:"String"},{name:"eventData",type:"Object",optional:true},{name:"handler",type:"Function"}]}],triggerHandler:[{added:"1.2",arg:[{name:"eventType",type:"String"},{name:"extraParameters",type:"Array",optional:true}]}],trigger:[{added:"1.0",arg:[{name:"eventType",type:"String"},{name:"extraParameters",type:"Array"}]}],ajaxComplete:[{added:"1.0",arg:[{name:"handler(event, XMLHttpRequest, ajaxOptions)",type:"Function"}]}],one:[{added:"1.1",arg:[{name:"eventType",type:"String"},{name:"eventData",type:"Object",optional:true},{name:"handler(eventObject)",type:"Function"}]}],serializeArray:[{added:"1.2"}],serialize:[{added:"1.0"}],"jQuery.ajaxSetup":[{added:"1.1",arg:[{name:"options",type:"Options"}]}],ajaxSuccess:[{added:"1.0",arg:[{name:"handler(event, XMLHttpRequest, ajaxOptions)",type:"Function"}]}],ajaxStop:[{added:"1.0",arg:[{name:"handler()",type:"Function"}]}],ajaxStart:[{added:"1.0",arg:[{name:"handler()",type:"Function"}]}],ajaxSend:[{added:"1.0",arg:[{name:"handler(event, XMLHttpRequest, ajaxOptions)",type:"Function"}]}],ajaxError:[{added:"1.0",arg:[{name:"handler(event, XMLHttpRequest, ajaxOptions, thrownError)",type:"Function"}]}],unbind:[{added:"1.0",arg:[{name:"eventType",type:"String"},{name:"handler(eventObject)",type:"Function"}]},{added:"1.0",arg:[{name:"event",type:"Object"}]}],bind:[{added:"1.0",arg:[{name:"eventType",type:"String"},{name:"eventData",type:"Object",optional:true},{name:"handler(eventObject)",type:"Function"}]},{added:"1.4",arg:[{name:"events",type:"Object"}]}],slice:[{added:"1.1.4",arg:[{name:"start",type:"Integer"}]},{added:"1.1.4",arg:[{name:"end",type:"Integer",optional:true}]}],jQuery:[{added:"1.0",arg:[{name:"selector",type:"selector"},{name:"context",type:"Element, jQuery",optional:true}]},{added:"1.0",arg:[{name:"element",type:"Element"}]},{added:"1.0",arg:[{name:"elementArray",type:"Array"}]},{added:"1.0",arg:[{name:"jQuery object",type:"Object"}]},{added:"1.4"},{added:"1.0",arg:[{name:"html",type:"String"},{name:"ownerDocument",type:"document",optional:true}]},{added:"1.4",arg:[{name:"html",type:"String"},{name:"props",type:"Object"}]},{added:"1.0",arg:[{name:"callback",type:"Function"}]}],stop:[{added:"1.2",arg:[{name:"clearQueue",type:"Boolean",optional:true},{name:"jumpToEnd",type:"Boolean",optional:true}]}],end:[{added:"1.0"}],andSelf:[{added:"1.2"}],siblings:[{added:"1.0",arg:[{name:"selector",type:"Selector",optional:true}]}],animate:[{added:"1.0",arg:[{name:"properties",type:"Options"},{name:"duration",type:"String,Number",optional:true},{name:"easing",type:"String",optional:true},{name:"callback",type:"Callback",optional:true}]},{added:"1.0",arg:[{name:"properties",type:"Options"},{name:"options",type:"Options"}]}],prevAll:[{added:"1.2",arg:[{name:"selector",type:"Selector",optional:true}]}],prev:[{added:"1.0",arg:[{name:"selector",type:"Selector",optional:true}]}],fadeTo:[{added:"1.0",arg:[{name:"duration",type:"String,Number"},{name:"opacity",type:"Number"},{name:"callback",type:"Callback",optional:true}]}],fadeOut:[{added:"1.0",arg:[{name:"duration",type:"String,Number",optional:true},{name:"callback",type:"Callback",optional:true}]}],parents:[{added:"1.0",arg:[{name:"selector",type:"Selector",optional:true}]}],fadeIn:[{added:"1.0",arg:[{name:"duration",type:"String,Number",optional:true},{name:"callback",type:"Callback",optional:true}]}],offsetParent:[{added:"1.26"}],slideToggle:[{added:"1.0",arg:[{name:"duration",type:"String,Number",optional:true},{name:"callback",type:"Callback",optional:true}]}],"jQuery.post":[{added:"1.0",arg:[{name:"url",type:"String"},{name:"data",type:"Map, String",optional:true},{name:"success(data, textStatus)",type:"Function",optional:true},{name:"dataType",type:"String",optional:true}]}],slideUp:[{added:"1.0",arg:[{name:"duration",type:"String,Number",optional:true},{name:"callback",type:"Callback",optional:true}]}],nextAll:[{added:"1.2",arg:[{name:"selector",type:"String",optional:true}]}],next:[{added:"1.0",arg:[{name:"selector",type:"Selector",optional:true}]}],slideDown:[{added:"1.0",arg:[{name:"duration",type:"String,Number",optional:true},{name:"callback",type:"Callback",optional:true}]}],find:[{added:"1.0",arg:[{name:"selector",type:"Selector"}]}],"jQuery.getScript":[{added:"1.0",arg:[{name:"url",type:"String"},{name:"success(data, textStatus)",type:"Function",optional:true}]}],contents:[{added:"1.2"}],closest:[{added:"1.3",arg:[{name:"selector",type:"Selector"}]},{added:"1.4",arg:[{name:"selector",type:"Selector"},{name:"context",type:"Element",optional:true}]},{added:"1.4",arg:[{name:"selectors",type:"Array"},{name:"context",type:"Element",optional:true}]}],"jQuery.getJSON":[{added:"1.0",arg:[{name:"url",type:"String"},{name:"data",type:"Map",optional:true},{name:"callback(data, textStatus)",type:"Function",optional:true}]}],"jQuery.get":[{added:"1.0",arg:[{name:"url",type:"String"},{name:"data",type:"Map, String",optional:true},{name:"callback(data, textStatus, XMLHttpRequest)",type:"Function",optional:true},{name:"dataType",type:"String",optional:true}]}],"jQuery.ajax":[{added:"1.0",arg:[{name:"settings",type:"Map"}]}],length:[{added:"1.0"}],children:[{added:"1.0",arg:[{name:"selector",type:"Selector",optional:true}]}],selector:[{added:"1.3"}],add:[{added:"1.0",arg:[{name:"selector",type:"Selector"}]},{added:"1.0",arg:[{name:"elements",type:"Elements"}]},{added:"1.0",arg:[{name:"html",type:"HTML"}]},{added:"1.4",arg:[{name:"selector",type:"Selector"},{name:"context",type:"Element"}]}],context:[{added:"1.3"}],outerWidth:[{added:"1.2.6",arg:[{name:"includeMargin",type:"Boolean",optional:true}]}],outerHeight:[{added:"1.2.6",arg:[{name:"includeMargin",type:"Boolean",optional:true}]}],toggle:[{added:"1.0",arg:[{name:"handler(eventObject)",type:"Function"},{name:"handler(eventObject)",type:"Function"},{name:"handler(eventObject)",type:"Function",optional:true}]},{added:"1.0",arg:[{name:"duration",type:"String,Number",optional:true},{name:"callback",type:"Callback",optional:true}]},{added:"1.3",arg:[{name:"showOrHide",type:"Boolean"}]}],innerWidth:[{added:"1.2.6"}],innerHeight:[{added:"1.2.6"}],"jQuery.param":[{added:"1.2",arg:[{name:"obj",type:"Array, Object"}]},{added:"1.4",arg:[{name:"obj",type:"Array, Object"},{name:"traditional",type:"Boolean"}]}],hide:[{added:"1.0"},{added:"1.0",arg:[{name:"duration",type:"String,Number"},{name:"callback",type:"",optional:true}]}],width:[{added:"1.0"},{added:"1.0",arg:[{name:"value",type:"String, Number"}]}],height:[{added:"1.0"},{added:"1.0",arg:[{name:"value",type:"String, Number"}]}],show:[{added:"1.0"},{added:"1.0",arg:[{name:"duration",type:"String,Number"},{name:"callback",type:"Callback",optional:true}]}],scrollLeft:[{added:"1.2.6"},{added:"1.2.6",arg:[{name:"value",type:"Number"}]}],"jQuery.trim":[{added:"1.0"}],"jQuery.isFunction":[{added:"1.2"}],"jQuery.isArray":[{added:"1.3",arg:[{name:"obj",type:"Object"}]}],"jQuery.unique":[{added:"1.1.3",arg:[{name:"array",type:"Array"}]}],"jQuery.merge":[{added:"1.0",arg:[{name:"first",type:"Array"},{name:"second",type:"Array"}]}],"jQuery.inArray":[{added:"1.2",arg:[{name:"value",type:"Any"},{name:"array",type:"Array"}]}],"jQuery.map":[{added:"1.0",arg:[{name:"array",type:"Array"},{name:"callback(elementOfArray, indexInArray)",type:"Function"}]}],"jQuery.makeArray":[{added:"1.2",arg:[{name:"obj",type:"Object"}]}],"jQuery.grep":[{added:"1.0",arg:[{name:"array",type:"Array"},{name:"function(elementOfArray, indexInArray)",type:"Function"},{name:"invert",type:"Boolean",optional:true}]}],"jQuery.extend":[{added:"1.0",arg:[{name:"target",type:"Object"},{name:"object1",type:"Object",optional:true},{name:"objectN",type:"Object",optional:true}]},{added:"1.1.4",arg:[{name:"deep",type:"Boolean",optional:true},{name:"target",type:"Object"},{name:"object1",type:"Object"},{name:"objectN",type:"Object",optional:true}]}],"jQuery.each":[{added:"1.0",arg:[{name:"object",type:"Object"},{name:"callback(indexInArray, valueOfElement)",type:"Function"}]}],"jQuery.boxModel":[{added:"1.0"}],scrollTop:[{added:"1.2.6"},{added:"1.2.6",arg:[{name:"value",type:"Number"}]}],"jQuery.support":[{added:"1.3"}],position:[{added:"1.2"}],offset:[{added:"1.2"},{added:"1.4",arg:[{name:"coordinates",type:"Object"}]},{added:"1.4",arg:[{name:"function(index, coords)",type:"Function"}]}],css:[{added:"1.0",arg:[{name:"propertyName",type:"String"}]},{added:"1.0",arg:[{name:"propertyName",type:"String"},{name:"value",type:"String, Number"}]},{added:"1.4",arg:[{name:"propertyName",type:"String"},{name:"function(index, value)",type:"Function"}]},{added:"1.0",arg:[{name:"map",type:"Map"}]}],unwrap:[{added:"1.4"}],detach:[{added:"1.4",arg:[{name:"selector",type:"Selector",optional:true}]}],clone:[{added:"1.0",arg:[{name:"withDataAndEvents",type:"Boolean",optional:true}]}],remove:[{added:"1.0",arg:[{name:"selector",type:"String",optional:true}]}],replaceAll:[{added:"1.2"}],replaceWith:[{added:"1.2",arg:[{name:"newContent",type:"String, Element, jQuery"}]},{added:"1.4",arg:[{name:"function",type:"Function"}]}],wrapInner:[{added:"1.2",arg:[{name:"wrappingElement",type:"String"}]},{added:"1.4",arg:[{name:"wrappingFunction",type:"Function"}]}],wrapAll:[{added:"1.2",arg:[{name:"wrappingElement",type:"String, Selector, Element, jQuery"}]}],wrap:[{added:"1.0",arg:[{name:"wrappingElement",type:"String, Selector, Element, jQuery"}]},{added:"1.4",arg:[{name:"wrappingFunction",type:"Function"}]}],insertBefore:[{added:"1.0",arg:[{name:"target",type:"Selector, Element, jQuery"}]}],before:[{added:"1.0",arg:[{name:"content",type:"String, Element, jQuery"}]},{added:"1.4",arg:[{name:"function",type:"Function"}]}],insertAfter:[{added:"1.0",arg:[{name:"target",type:"Selector, Element, jQuery"}]}],after:[{added:"1.0",arg:[{name:"content",type:"String, Element, jQuery"}]},{added:"1.4",arg:[{name:"function",type:"Function"}]}],prependTo:[{added:"1.0",arg:[{name:"target",type:"Selector, Element, jQuery"}]}],prepend:[{added:"1.0",arg:[{name:"content",type:"String, Element, jQuery"}]},{added:"1.4",arg:[{name:"function(index, html)",type:"Function"}]}],appendTo:[{added:"1.0",arg:[{name:"target",type:"Selector, Element, jQuery"}]}],append:[{added:"1.0",arg:[{name:"content",type:"String, Element, jQuery"}]},{added:"1.4",arg:[{name:"function(index, html)",type:"Function"}]}],val:[{added:"1.0"},{added:"1.0",arg:[{name:"value",type:"String"}]},{added:"1.4",arg:[{name:"function",type:"Function"}]}],html:[{added:"1.0"},{added:"1.0",arg:[{name:"htmlString",type:"String"}]},{added:"1.4",arg:[{name:"function(index, html)",type:"Function"}]}],map:[{added:"1.2",arg:[{name:"callback(index, domElement)",type:"Function"}]}],is:[{added:"1.0",arg:[{name:"selector",type:"Selector"}]}],filter:[{added:"1.0",arg:[{name:"selector",type:"Selector"}]},{added:"1.0",arg:[{name:"function(index)",type:"Function"}]}],toggleClass:[{added:"1.0",arg:[{name:"className",type:"String"}]},{added:"1.3",arg:[{name:"className",type:"String"},{name:"switch",type:"Boolean"}]},{added:"1.4",arg:[{name:"function(index, class)",type:"Function"},{name:"switch",type:"Boolean",optional:true}]}],removeClass:[{added:"1.0",arg:[{name:"className",type:"String",optional:true}]},{added:"1.4",arg:[{name:"function(index, class)",type:"Function"}]}],hasClass:[{added:"1.2",arg:[{name:"className",type:"String"}]}],removeAttr:[{added:"1.0",arg:[{name:"attributeName",type:"String"}]}],attr:[{added:"1.0",arg:[{name:"attributeName",type:"String"}]},{added:"1.0",arg:[{name:"attributeName",type:"String"},{name:"value",type:"Object"}]},{added:"1.0",arg:[{name:"map",type:"Map"}]},{added:"1.1",arg:[{name:"attributeName",type:"String"},{name:"function(index, attr)",type:"Function"}]}],addClass:[{added:"1.0",arg:[{name:"className",type:"String"}]},{added:"1.4",arg:[{name:"function(index, class)",type:"Function"}]}]};
    
    if ( !_jQuery ) {
        return;
    }
    
    lint.api = api;
    
    // Correct API
    // Yes, it's ugly, but necessary...
    api['jQuery.data'][1].arg[1].optional = true; // Making $.data(.,THIS) optional
    api.each[0].arg[1] = api['jQuery.each'][0].arg[2] = {name:'args', type:'Array', optional:true};
    api['jQuery.data'][0].arg[2].type = '*';
    api.attr[1].arg[1].type = '*';
    api.data[0].arg[1].type = '*';
    api['jQuery.each'][0].arg[0].type += ', Array';
    // extraParam to trigger & triggerHandler IS optional
    api.trigger[0].arg[1].optional = true;
    api.triggerHandler[0].arg[1].optional = true;
    api.slice[0].arg[1] = {name:'end',type:'Integer',optional:true};
    // Add elem arg to start of args for jQuery.queue
    var jQQueue = api['jQuery.queue'];
    jQQueue[0].arg.unshift({type:'Element', name:'elem'});
    jQQueue[1].arg.unshift({type:'Element', name:'elem'});
    jQQueue[2].arg.unshift({type:'Element', name:'elem'});
    api['jQuery.removeData'][0].arg.unshift({type:'Element', name:'elem'});
    // one(), bind(), unbind():
    api.one[1] = api.unbind[1] = api.bind[1];
    // $().bind({}, {data});
    api.one[1].arg[1] = api.bind[1].arg[1] = _jQuery.extend({}, api.one[0].arg[1], {type:'*'});
    // Make handler optional to unbind:
    api.unbind[0].arg[1].optional = true;
    api.unbind.push({added:'1.3'});
    api.hover[1] = {added:'1.4',arg:[{name:'handlerInOut(eventObject)',type:'Function'}]};
    api['jQuery.proxy'][0].arg[1].optional = true;
    api.bind[0].arg[1].type = 'notFunction';
    // Make append('a','b','c') (and prepend) possible:
    api.append[0].arg[0].multiple = true;
    api.prepend[0].arg[0].multiple = true;
    
    var version = _jQuery.fn.jquery,
        map = _jQuery.map,
        each = _jQuery.each,
        extend = _jQuery.extend,
        find = _jQuery.find,
        locale = langs[lint.lang],
        undefined,
        slice = function(a,s,e) {
            return a.length ? Array.prototype.slice.call(a, s || 0, e || a.length) : [];
        },
        compare = function(a,b) {
            
            // Compare two arrays
            
            var i = a.length;
            
            if (a.length !== b.length) {
                return false;
            }
            
            while (i--) {
                if (a[i] !== b[i]) {
                    return false;
                }
            }
            
            return true;
        
        },
        isFunction = function(obj) {
            return toString.call(obj) === "[object Function]";
        },
        isArray = function(obj) {
            return toString.call(obj) === "[object Array]";   
        },
        toString = Object.prototype.toString,
        typeToString = function(o) {
            
            if (!o) { return ""; }
            
            if (typeof o === 'string') {
                return '"' + o.replace(/"/g,'\\"') + '"';
            }
            
            if (isFunction(o)) {
                return 'function(){...}';
            }
            
            return o.toString();
        },
        shaveArray = function(arr) {
            arr = Array.prototype.slice.call(arr);
            // Shave "undefined" off the end of args
            for (var i = arr.length; i--;) {
                if (arr[i] === undefined) {
                    arr.splice(i, 1);
                } else {
                    break;
                }
            }
            return arr;
        },
        // type map
        types = {
            '*': function() {
                return true;
            },
            selector: function(o) {
                return this.string(o);
            },
            element: function(o) {
                return o && (!!o.nodeName || o === window);
            },
            elements: function(o) {
                return this.element(o) || this.jquery(o) || this.array(o);
            },
            array: function(o) {
                // Just check that it's "array-like"
                return o && o.length !== undefined
                        && typeof o !== 'string' && !isFunction(o);
            },
            jquery: function(o) {
                return o instanceof _jQuery;
            },
            object: function(o) {
                return toString.call(o) === '[object Object]';
            },
            'function': function(o) {
                return isFunction(o);
            },
            notfunction: function(o) {
                return !this['function'](o);
            },
            callback: function(o) {
                return isFunction(o);
            },
            string: function(o) {
                return typeof o === 'string';
            },
            number: function(o) {
                return typeof o === 'number' && !isNaN(o);
            },
            integer: function(o) {
                return this.number(o) && ~~o === o;
            },
            map: function(o) {
                return this.object(o);
            },
            options: function(o) {
                return this.object(o);
            },
            'null': function(o) {
                return o === null;
            },
            'boolean': function(o) {
                return typeof o === 'boolean';
            }
        },
        typeCheck = function typeCheck(type, arg) {
            
            // Check that argument is of the right type
            // The types are specified within the API data
            
            var split = type.split(/,\s?/g),
                i = split.length,
                cur;
                
            if (arg === undefined) {
                return false;
            }
            
            while (i--) {
                cur = split[i];
                if ((types[cur] && types[cur](arg))
                    // Try lowercase too
                    || (types[cur = cur.toLowerCase()] && types[cur](arg))) {
                    return true;
                }
            }
                
            return false;
                
        },
        complies = function complies(args, sig) {
            
            // Determine if argument list complies with
            // signature outlined in API.
            
            var matches = false,
                sigArg,
                argLength = args.length,
                nextIsOptional = false;
            
            if (version < sig.added) {
                // Too new
                return false;
            }
            
            if (!sig.arg) {
                return 0 === args.length;
            }
            
            if (!sig.arg[0] && (args.length > 1)) {
                return false;
            }
            
            for (
                    var sigIndex = 0,
                        argIndex = 0,
                        fullLength = Math.max(argLength,sig.arg.length||1);
                    sigIndex < fullLength || argIndex < argLength;
                    ++sigIndex
                ) {
                
                sigArg = sigIndex === 0 ? sig.arg[0] || sig.arg : sig.arg[sigIndex];
                
                if (!sigArg) {
                    // Too many args
                    return false;
                }
                
                matches = typeCheck(sigArg.type, args[argIndex]);
                
                if (!matches) {
                    if (sigArg.optional) {
                        if (args[argIndex] === undefined || args[argIndex] === null) {
                            ++argIndex;
                            matches = true;
                        }
                        continue;
                    } else {
                        // Sig isn't optional, return false
                        return false;
                    }
                }
                
                if (sigArg.multiple) {
                    // If it's multiple, then carry on with the same
                    // signature, but check that there are remaining
                    // arguments
                    --sigIndex;
                    if (argIndex + 1 >= argLength) {
                        break;
                    }
                }
                
                ++argIndex;
                
            }
            
            return matches;
            
        },
        logLocation = function() {
            try {
                throw new Error();
            } catch(e) {
                if (e.stack) {
                    lint.console.groupCollapsed(locale.location);
                    lint.console.log(
                        e.stack
                            // Remove everything before the file name and line number
                            // plus, get rid of errors from jQuery.lint.js & any libs
                            // from google's CDN
                            .replace(/^.+?\n|.+?(jquery\.lint\.js|http:\/\/ajax\.googleapis\.com).+?(\n|$)|.+?(?=@)/g, '')
                            // Remove duplicates
                            .replace(/(^|\n)(.+?)\n(?=\2(?:\n|$)|[\s\S]+?\n\2(?:\n|$))/g, '$1')
                    );
                    lint.console.groupEnd();
                }
            }
        },
        selectorCache = {},
        internal = false;
        
    function register(name, methodAPI) {
        
        var obj = /^jQuery\./.test(name) ? _jQuery : _jQuery.fn,
            methodName = name.replace(/^jQuery\./, '');
            
        obj[methodName] = (function(meth, name){
            return extend(function() {
                return coverMethod.call(this, name, function(){
                    var wasInternal = internal;
                    internal = true;
                    try {
                        var ret = meth.apply(this, arguments);
                    } catch(e) {
                        internal = wasInternal;
                        throw e;
                    }
                    internal = wasInternal;
                    return ret;
                }, arguments);
            }, meth);
        })(obj[methodName], name);
        
        if (methodAPI) {
            api[name] = methodAPI;
        }
        
    }
    
    lint.register = register;
    
    function coverMethod(name, meth, args) {
        
        args = shaveArray(args);
        
        var sigs = api[name],
            _console = lint.console,
            self = this,
            i = 0,
            sig,
            specialCheckResults = (function(){
                
                // Perform special checks for current level and
                // all levels below current level.
                
                var lvl = lint.level + 1,
                    checks = [],
                    check;
                    
                while (lvl--) {
                    if (specialChecks[lvl] && (check = specialChecks[lvl][name])) {
                        if (types.array(check)) {
                            each(check, function(i, chk){
                                checks.push(
                                    chk.apply(self, args)
                                );
                            })
                        } else {
                            checks.push(
                                check.apply(self, args)
                            );
                        }
                    }
                }
                
                return checks;
                
            })(),
            signatureMatch = false,
            sliced = slice(this, 0, 10);
        
        if (!sigs || !lint.level || internal) {
            return meth.apply(this, args);
        }
        
        if (this.length > 10) {
            sliced.push('...');
        }
        
        // Check for calls like css().css().css()
        // May as well use css({...})
        if (lint.level > 2 && !types.object(args[0]) && !isFunction(args[1]) && (/^(css|attr)$/.test(name) || (name === 'bind' && version >= '1.4'))) {
            
            if (this._lastMethodCalled === name) {
                _console.warn(locale.methodTwice.replace(/%0/, name));
                _console.groupCollapsed(locale.moreInfo);
                    if (this instanceof _jQuery) {
                        _console.log(locale.collection, sliced);
                    }
                    _console.log(args);
                    _console.log(
                        locale.combineCalls
                            .replace(/%0/, name)
                            .replace(/%1/, '{\n' +
                                map([args, this._lastMethodArgs], function(a){
                                    return '  "' + a[0] + '": ' + typeToString(a[1]);
                                }).join(',\n')
                            + '\n}')
                    );
                _console.groupEnd();
            }
            
            this._lastMethodCalled = name;
            this._lastMethodArgs = args;
            setTimeout(function(){
                self._lastMethodCalled = null;
                self._lastMethodArgs = null;
            });
            
        }
        
        // Check all arguments passed to method for compliance
        // against the corresponding signature.
        while ((sig = sigs[i++])) {
            if ( complies(args, sig) ) {
                signatureMatch = true;
                break;
            }
        }
        
        if (!signatureMatch) {
            
            try {
                
                // Args !== signature
                _console.warn(locale.incorrectCall.replace(/%0/, name));
                _console.groupCollapsed(locale.moreInfo);
                    if (this instanceof _jQuery) {
                        _console.log(locale.collection, sliced);
                    }
                    logLocation();
                    _console.log(locale.youPassed, args);
                    _console.group(locale.availableSigsInclude);
                        each(sigs, function(i, sig){
                            if (version < sig.added) {
                                return;
                            }
                            var sigArgs = sig.arg;
                            _console.log(
                                name + '(' +
                                (sigArgs ?
                                     sigArgs[0] ?
                                        map(sigArgs, function(sig, i){
                                            return sig ? sig.optional ? '[' + sig.name + ']' : sig.multiple ? sig.name + ',[...]' : sig.name : [];
                                        }).join(', ') :
                                        sigArgs.name
                                : '') + ')'
                            );
                        });
                    _console.groupEnd();
                _console.groupEnd();
                
            } catch(e) { }
            
        }
        
        try {
            if (specialCheckResults.length) {
                each(specialCheckResults, function(i, checkResult){
                    if (checkResult && checkResult !== true) {
                        if (isFunction(checkResult)) {
                            checkResult(_console);
                        } else {
                            _console.warn(locale.specialCheckFailed.replace(/%0/, name));
                            _console.groupCollapsed(locale.moreInfo);
                                _console.log(checkResult);
                                if (self instanceof _jQuery) {
                                    _console.log(locale.collection, sliced);
                                }
                                logLocation();
                            _console.groupEnd();
                        }
                    }
                });
            }
        } catch(e) { }
        
        if (lint['throw']) {
            return meth.apply(this, args);
        }
        
        try {
            return meth.apply(this, args);
        } catch(e) {
            
            try {
                _console.warn(
                    locale.errorThrown.replace(/%0/, name), e
                );
                _console.groupCollapsed(locale.moreInfo);
                    logLocation();
                    _console.log(locale.youPassed, args);
                _console.groupEnd();
            } catch(e) { }
            return this;
        }
        
    }
    
    // "Cover" init constructor
    // Reports when no elements found, and when selector
    // used more than once to no effect.
    _jQuery.fn.init = (function(_init){
        
        return function(selector, context) {
            
            var ret = coverMethod.call(this, 'jQuery', function(){
                
                    // Set internal flag to avoid incorrect internal method
                    // calls being reported by Lint.
                    
                    var wasInternal = internal;
                    internal = true;
                    try {
                        var instance = new _init(selector, context);
                        extend(instance, this); // Add any flags (added before instantiation)
                    } catch(e) {
                        internal = wasInternal;
                        throw e;
                    }
                    internal = wasInternal;
                    
                    return instance
                
                }, arguments),
                _console = lint.console;
                
            // Deal with situations where no elements are returned
            // and for the same selector being used more than once
            // to no effect
                
            try{
                
                if (typeof selector === 'string' && lint.level > 1) {
                    
                    if (lint.specific.noElementsFound && !ret[0]) {
                        
                        // No elements returned
                        _console.warn(locale.noElementsFound.replace(/%0/, selector));
                    
                    } else {
                        
                        if (lint.specific.repeatSelector) {
                            
                            // Check for identical collection already in cache.
                            if ( selectorCache[selector] && compare(selectorCache[selector], ret) ) {
                                
                                _console.warn(locale.repeatSelector);
                                _console.groupCollapsed(locale.info);
                                    logLocation();
                                    _console.log(locale.selector + '"' + selector + '"');
                                    _console.log(locale.selectorAdvice);
                                _console.groupEnd();
                                
                            }
                            
                        }
                        
                    }
                    
                    selectorCache[selector] = ret;
                    
                }
                
            } catch(e) { }
            
            return ret;
        
        };
        
    })(_jQuery.fn.init);
    
    // Cover all methods, except init
    for (var i in _jQuery.fn) {
        
        if (i === 'init' || !isFunction(_jQuery.fn[i])) {
            continue;
        }
        
        register(i);
        
    }
    
    // Cover some helper function under jQ namespace
    for (var i in _jQuery) {
        
        if ( !jQNameSpace.test(i) || !isFunction(_jQuery[i]) ) {
            continue;
        }
        
        register('jQuery.' + i);
        
    }
    
    _jQuery.LINT = lint;
    
    /////////////////////////
    // Some special checks //
    /////////////////////////
    
    specialChecks[2].jQuery = [
        function(selector) {
            
            // Find invalid filters (e.g. :hover, :active etc.)
            // suggested by Paul Irish
            
            if (lint.specific.invalidFilters && typeof selector === 'string' && !/^[^<]*(<[\w\W]+>)[^>]*$/.test(selector)) {
                
                // It's a string, and NOT html - must be a selector
                
                var invalidFilters = [];
                
                selector.replace(/('|")(?:\\\1|[^\1])+?\1/g, '').replace(/:(\w+)/g, function(m, filter){
                    if (!/^(contains|not)$/.test(filter) && !((filter in _jQuery.expr[':']) || (filter in _jQuery.expr.setFilters))) {
                        invalidFilters.push(m);
                    }
                });
                
                if (invalidFilters.length) {
                    return locale.invalidFilters.replace(/%0/, selector).replace(/%1/, invalidFilters.join('\n'));
                }
                
            }
            
        },
        function() {
            
            // Set flag for ready() method, so we can check
            // for $().ready() - which should be $(document).ready()
            // suggested by Paul Irish
            
            if (!arguments.length) {
                this._lint_noArgs = true;
            }
            
        }
    ];
    
    specialChecks[2].ready = [
        function() {
            // If _lint_noArgs is set then this object
            // was instantiated with no args. I.e. $().ready()
            if (this._lint_noArgs) {
                return locale.badReadyCall;
            }
        }
    ];
   
})();