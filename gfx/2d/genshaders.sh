# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

fxc ShadersD2D.fx -nologo -FhShadersD2D.h -Tfx_4_0 -Vn d2deffect
fxc ShadersD2D1.hlsl -ESampleRadialGradientPS -nologo -Tps_4_0_level_9_3 -Fhtmpfile -VnSampleRadialGradientPS
cat tmpfile > ShadersD2D1.h
fxc ShadersD2D1.hlsl -ESampleRadialGradientA0PS -nologo -Tps_4_0_level_9_3 -Fhtmpfile -VnSampleRadialGradientA0PS
cat tmpfile >> ShadersD2D1.h
rm tmpfile
