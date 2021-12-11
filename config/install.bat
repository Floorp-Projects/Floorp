@echo off
rem This Source Code Form is subject to the terms of the Mozilla Public
rem License, v. 2.0. If a copy of the MPL was not distributed with this
rem file, You can obtain one at http://mozilla.org/MPL/2.0/.

@echo on

@if not exist %2\nul mkdir %2
@rm -f %2\%1
@cp %1 %2
