
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is the JavaScript 2 Prototype.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.   Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*
* Alternatively, the contents of this file may be used under the
* terms of the GNU Public License (the "GPL"), in which case the
* provisions of the GPL are applicable instead of those above.
* If you wish to allow use of your version of this file only
* under the terms of the GPL and not to allow others to use your
* version of this file under the NPL, indicate your decision by
* deleting the provisions above and replace them with the notice
* and other provisions required by the GPL.  If you do not delete
* the provisions above, a recipient may use your version of this
* file under either the NPL or the GPL.
*/

#ifdef _WIN32
#include "msvc_pragma.h"
#endif


#include <algorithm>
#include <assert.h>
#include <list>
#include <stack>

#include "world.h"
#include "utilities.h"
#include "js2value.h"

#include <map>
#include <algorithm>

#include "regexp.h"
#include "reader.h"
#include "parser.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"


namespace JavaScript {
namespace MetaData {

    // Establish the label's location in a bytecode container
    void Label::setLocation(BytecodeContainer *bCon, uint32 location)
    {
        mHasLocation = true;
        mLocation = location;
        for (std::vector<uint32>::iterator i = mFixupList.begin(), end = mFixupList.end(); 
                        (i != end); i++)
        {
            uint32 branchLocation = *i;
            bCon->setOffset(branchLocation, int32(mLocation - branchLocation)); 
        }
    }

    // Add a branch location to the list of fixups for this label
    // (or resolve the branch if the location is known already)
    void Label::addFixup(BytecodeContainer *bCon, uint32 branchLocation) 
    { 
        if (mHasLocation)
            bCon->addOffset(int32(mLocation - branchLocation));
        else {
            mFixupList.push_back(branchLocation); 
            bCon->addOffset(0);
        }
    }



    // Look up the pc and return a position from the map
    size_t BytecodeContainer::getPosition(uint16 pc)
    {
        for (std::vector<MapEntry>::iterator i = pcMap.begin(), end = pcMap.end(); (i != end); i++)
            if (i->first >= pc)
                return i->second;
        return 0;
    }


    // insert the opcode, marking it's position in the pcmap and
    // adjusting the stack as appropriate.
    void BytecodeContainer::emitOp(JS2Op op, size_t pos)
    { 
        adjustStack(op); 
        addByte((uint8)op); 
        pcMap.push_back(MapEntry((uint16)mBuffer.size(), pos)); 
    }

    // insert the opcode, marking it's position in the pcmap and
    // adjusting the stack as supplied.
    void BytecodeContainer::emitOp(JS2Op op, size_t pos, int32 effect)     
    {
        adjustStack(op, effect); 
        addByte((uint8)op); 
        pcMap.push_back(std::pair<uint16, size_t>((uint16)mBuffer.size(), pos)); 
    }

    // Track the high-water mark for the stack 
    // and watch for a bad negative stack
    void BytecodeContainer::adjustStack(JS2Op op, int32 effect)
    { 
        mStackTop += effect; 
        if (mStackTop > mStackMax) 
            mStackMax = mStackTop; 
        ASSERT(mStackTop >= 0); 
    }

    // get a new label
    BytecodeContainer::LabelID BytecodeContainer::getLabel()
    {
        LabelID result = mLabelList.size();
        mLabelList.push_back(Label());
        return result;
    }
    // set the current pc as needing a fixup to a label
    void BytecodeContainer::addFixup(LabelID label) 
    { 
        ASSERT(label < mLabelList.size());
        mLabelList[label].addFixup(this, mBuffer.size()); 
    }
    // set the current pc as the position for a label
    void BytecodeContainer::setLabel(LabelID label)
    {
        ASSERT(label < mLabelList.size());
        mLabelList[label].setLocation(this, mBuffer.size()); 
    }

    void BytecodeContainer::mark()
    {
        for (std::vector<Multiname *>::iterator mi = mMultinameList.begin(), mend = mMultinameList.end(); (mi != mend); mi++) {
            GCMARKOBJECT(*mi);   
        }
        for (std::vector<Frame *>::iterator fi = mFrameList.begin(), fend = mFrameList.end(); (fi != fend); fi++) {
            GCMARKOBJECT(*fi);   
        }
        for (std::vector<RegExpInstance *>::iterator ri = mRegExpList.begin(), rend = mRegExpList.end(); (ri != rend); ri++) {
            GCMARKOBJECT(*ri);   
        }
    }

}
}


