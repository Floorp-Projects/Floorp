//
// Copyright (c) 2002-2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// gl_FragColor needs to broadcast to all color buffers in ES2 if
// GL_EXT_draw_buffers is explicitly enabled in a fragment shader.
//
// We emulate this by replacing all gl_FragColor with gl_FragData[0], and in the end
// of main() function, assigning gl_FragData[1], ..., gl_FragData[maxDrawBuffers-1]
// with gl_FragData[0].
//

#include "compiler/translator/EmulateGLFragColorBroadcast.h"
#include "compiler/translator/IntermNode.h"

namespace
{

TIntermConstantUnion *constructIndexNode(int index)
{
    TConstantUnion *u = new TConstantUnion[1];
    u[0].setIConst(index);

    TType type(EbtInt, EbpUndefined, EvqConst, 1);
    TIntermConstantUnion *node = new TIntermConstantUnion(u, type);
    return node;
}

TIntermBinary *constructGLFragDataNode(int index)
{
    TIntermBinary *indexDirect = new TIntermBinary(EOpIndexDirect);
    TIntermSymbol *symbol      = new TIntermSymbol(0, "gl_FragData", TType(EbtFloat, 4));
    indexDirect->setLeft(symbol);
    TIntermConstantUnion *indexNode = constructIndexNode(index);
    indexDirect->setRight(indexNode);
    return indexDirect;
}

TIntermBinary *constructGLFragDataAssignNode(int index)
{
    TIntermBinary *assign = new TIntermBinary(EOpAssign);
    assign->setLeft(constructGLFragDataNode(index));
    assign->setRight(constructGLFragDataNode(0));
    assign->setType(TType(EbtFloat, 4));
    return assign;
}

class GLFragColorBroadcastTraverser : public TIntermTraverser
{
  public:
    GLFragColorBroadcastTraverser()
        : TIntermTraverser(true, false, false), mMainSequence(nullptr), mGLFragColorUsed(false)
    {
    }

    void broadcastGLFragColor(int maxDrawBuffers);

    bool isGLFragColorUsed() const { return mGLFragColorUsed; }

  protected:
    void visitSymbol(TIntermSymbol *node) override;
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;

  private:
    TIntermSequence *mMainSequence;
    bool mGLFragColorUsed;
};

void GLFragColorBroadcastTraverser::visitSymbol(TIntermSymbol *node)
{
    if (node->getSymbol() == "gl_FragColor")
    {
        queueReplacement(node, constructGLFragDataNode(0), OriginalNode::IS_DROPPED);
        mGLFragColorUsed = true;
    }
}

bool GLFragColorBroadcastTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    switch (node->getOp())
    {
        case EOpFunction:
            // Function definition.
            ASSERT(visit == PreVisit);
            if (node->getName() == "main(")
            {
                TIntermSequence *sequence = node->getSequence();
                ASSERT((sequence->size() == 1) || (sequence->size() == 2));
                if (sequence->size() == 2)
                {
                    TIntermAggregate *body = (*sequence)[1]->getAsAggregate();
                    ASSERT(body);
                    mMainSequence = body->getSequence();
                }
            }
            break;
        default:
            break;
    }
    return true;
}

void GLFragColorBroadcastTraverser::broadcastGLFragColor(int maxDrawBuffers)
{
    ASSERT(maxDrawBuffers > 1);
    if (!mGLFragColorUsed)
    {
        return;
    }
    ASSERT(mMainSequence);
    // Now insert statements
    //   gl_FragData[1] = gl_FragData[0];
    //   ...
    //   gl_FragData[maxDrawBuffers - 1] = gl_FragData[0];
    for (int colorIndex = 1; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        mMainSequence->insert(mMainSequence->end(), constructGLFragDataAssignNode(colorIndex));
    }
}

}  // namespace anonymous

void EmulateGLFragColorBroadcast(TIntermNode *root,
                                 int maxDrawBuffers,
                                 std::vector<sh::OutputVariable> *outputVariables)
{
    ASSERT(maxDrawBuffers > 1);
    GLFragColorBroadcastTraverser traverser;
    root->traverse(&traverser);
    if (traverser.isGLFragColorUsed())
    {
        traverser.updateTree();
        traverser.broadcastGLFragColor(maxDrawBuffers);
        for (auto &var : *outputVariables)
        {
            if (var.name == "gl_FragColor")
            {
                // TODO(zmo): Find a way to keep the original variable information.
                var.name       = "gl_FragData";
                var.mappedName = "gl_FragData";
                var.arraySize  = maxDrawBuffers;
            }
        }
    }
}
