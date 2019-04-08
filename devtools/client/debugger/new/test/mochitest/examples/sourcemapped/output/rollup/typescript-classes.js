var rollupTypescriptClasses = (function () {
    'use strict';

    /*! *****************************************************************************
    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the Apache License, Version 2.0 (the "License"); you may not use
    this file except in compliance with the License. You may obtain a copy of the
    License at http://www.apache.org/licenses/LICENSE-2.0

    THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
    KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
    WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
    MERCHANTABLITY OR NON-INFRINGEMENT.

    See the Apache Version 2.0 License for specific language governing permissions
    and limitations under the License.
    ***************************************************************************** */
    /* global Reflect, Promise */

    var extendStatics = function(d, b) {
        extendStatics = Object.setPrototypeOf ||
            ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
            function (d, b) { for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p]; };
        return extendStatics(d, b);
    };

    function __extends(d, b) {
        extendStatics(d, b);
        function __() { this.constructor = d; }
        d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
    }

    function __decorate(decorators, target, key, desc) {
        var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
        if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
        else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
        return c > 3 && r && Object.defineProperty(target, key, r), r;
    }

    function decoratorFactory(opts) {
        return function decorator(target) {
            return target;
        };
    }

    // This file essentially reproduces an example Angular component to map testing,
    var AppComponent = /** @class */ (function () {
        function AppComponent() {
            this.title = 'app';
        }
        AppComponent = __decorate([
            decoratorFactory({
                selector: 'app-root'
            })
        ], AppComponent);
        return AppComponent;
    }());
    var fn = function (arg) {
        console.log("here");
    };
    fn("arg");
    var AnotherThing = /** @class */ (function () {
        function AnotherThing() {
            this.prop = 4;
        }
        return AnotherThing;
    }());
    var SubDecl = /** @class */ (function (_super) {
        __extends(SubDecl, _super);
        function SubDecl() {
            var _this = _super !== null && _super.apply(this, arguments) || this;
            _this.prop = 4;
            return _this;
        }
        return SubDecl;
    }(AnotherThing));
    var SubVar = /** @class */ (function (_super) {
        __extends(SubExpr, _super);
        function SubExpr() {
            var _this = _super !== null && _super.apply(this, arguments) || this;
            _this.prop = 4;
            return _this;
        }
        return SubExpr;
    }(AnotherThing));
    function test () {
        // This file is specifically for testing the mappings of classes and things
        // above, which means we don't want to include _other_ references to then.
        // To avoid having them be optimized out, we include a no-op eval.
        eval("");
        console.log("pause here");
    }

    return test;

}());
//# sourceMappingURL=typescript-classes.js.map
