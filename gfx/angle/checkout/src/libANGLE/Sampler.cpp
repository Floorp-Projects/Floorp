//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Sampler.cpp : Implements the Sampler class, which represents a GLES 3
// sampler object. Sampler objects store some state needed to sample textures.

#include "libANGLE/Sampler.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/SamplerImpl.h"

namespace gl
{

Sampler::Sampler(rx::GLImplFactory *factory, GLuint id)
    : RefCountObject(id), mState(), mImpl(factory->createSampler(mState)), mLabel()
{}

Sampler::~Sampler()
{
    SafeDelete(mImpl);
}

void Sampler::onDestroy(const Context *context) {}

void Sampler::setLabel(const Context *context, const std::string &label)
{
    mLabel = label;
}

const std::string &Sampler::getLabel() const
{
    return mLabel;
}

void Sampler::setMinFilter(GLenum minFilter)
{
    mState.setMinFilter(minFilter);
}

GLenum Sampler::getMinFilter() const
{
    return mState.getMinFilter();
}

void Sampler::setMagFilter(GLenum magFilter)
{
    mState.setMagFilter(magFilter);
}

GLenum Sampler::getMagFilter() const
{
    return mState.getMagFilter();
}

void Sampler::setWrapS(GLenum wrapS)
{
    mState.setWrapS(wrapS);
}

GLenum Sampler::getWrapS() const
{
    return mState.getWrapS();
}

void Sampler::setWrapT(GLenum wrapT)
{
    mState.setWrapT(wrapT);
}

GLenum Sampler::getWrapT() const
{
    return mState.getWrapT();
}

void Sampler::setWrapR(GLenum wrapR)
{
    mState.setWrapR(wrapR);
}

GLenum Sampler::getWrapR() const
{
    return mState.getWrapR();
}

void Sampler::setMaxAnisotropy(float maxAnisotropy)
{
    mState.setMaxAnisotropy(maxAnisotropy);
}

float Sampler::getMaxAnisotropy() const
{
    return mState.getMaxAnisotropy();
}

void Sampler::setMinLod(GLfloat minLod)
{
    mState.setMinLod(minLod);
}

GLfloat Sampler::getMinLod() const
{
    return mState.getMinLod();
}

void Sampler::setMaxLod(GLfloat maxLod)
{
    mState.setMaxLod(maxLod);
}

GLfloat Sampler::getMaxLod() const
{
    return mState.getMaxLod();
}

void Sampler::setCompareMode(GLenum compareMode)
{
    mState.setCompareMode(compareMode);
}

GLenum Sampler::getCompareMode() const
{
    return mState.getCompareMode();
}

void Sampler::setCompareFunc(GLenum compareFunc)
{
    mState.setCompareFunc(compareFunc);
}

GLenum Sampler::getCompareFunc() const
{
    return mState.getCompareFunc();
}

void Sampler::setSRGBDecode(GLenum sRGBDecode)
{
    mState.setSRGBDecode(sRGBDecode);
}

GLenum Sampler::getSRGBDecode() const
{
    return mState.getSRGBDecode();
}

void Sampler::setBorderColor(const ColorGeneric &color)
{
    mState.setBorderColor(color);
}

const ColorGeneric &Sampler::getBorderColor() const
{
    return mState.getBorderColor();
}

const SamplerState &Sampler::getSamplerState() const
{
    return mState;
}

rx::SamplerImpl *Sampler::getImplementation() const
{
    return mImpl;
}

void Sampler::syncState(const Context *context)
{
    // TODO(jmadill): Use actual dirty bits for sampler.
    mImpl->syncState(context);
}

}  // namespace gl
