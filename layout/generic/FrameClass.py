# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class FrameClass:
    def __init__(self, cls):
        self.cls = cls


class Frame(FrameClass):
    def __init__(self, cls, ty, flags):
        FrameClass.__init__(self, cls)
        self.ty = ty
        self.flags = flags
        self.is_concrete = True


class AbstractFrame(FrameClass):
    def __init__(self, cls):
        FrameClass.__init__(self, cls)
        self.is_concrete = False
