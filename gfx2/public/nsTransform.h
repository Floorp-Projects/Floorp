/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsMatrix.h"

/**
 * @file nsTransform.h
 */

/**
 * nsTransform class
 * @class nsTransform
 *
 * @see nsMatrix
 */
class nsTransform {
public:
  nsTransform();
  virtual ~nsTransform();

  /**
   * Transform types
   */
  //@{

  /** 
   * The unit type is not one of predefined types. It is invalid to attempt to define a
   * new value of this type or to attempt to switch an existing value to this type.
   */
  enum {
    NS_TRANSFORM_UNKNOWN   = 0,

    NS_TRANSFORM_IDENTITY  = 1,

    /**
     * A "matrix(...)" transformation.
     */
    NS_TRANSFORM_MATRIX    = 2,

    /**
     * A "translate(...)" transformation.
     */
    NS_TRANSFORM_TRANSLATE = 3,

    /**
     * A "scale(...)" transformation.
     */
    NS_TRANSFORM_SCALE     = 4,

    /**
     * A "rotate(...)" transformation.
     */
    NS_TRANSFORM_ROTATE    = 5,

    /**
     * A "skewX(...)" transformation.
     */
    NS_TRANSFORM_SKEWX     = 6,

    /**
     * A "skewY(...)" transformation.
     */
    NS_TRANSFORM_SKEWY     = 7
  };
  //@}

  unsigned short GetType() const;
  nsMatrix *GetMatrix() const;
  float GetAngle() const;

  /**
   * Sets the transform type to NS_TRANSFORM_IDENTITY.
   */
  void SetIdentity();

  /**
   * Sets the transform type to NS_TRANSFORM_MATRIX, with parameter matrix defining the new transformation.
   *
   * @param matrix The new matrix for the transformation.
   */
  void SetMatrix(nsMatrix *matrix);

  /**
   * Sets the transform type to NS_TRANSFORM_TRANSLATE, with parameters tx and ty defining the translation amounts
   *
   * @param tx The translation amount in X.
   * @param ty The translation amount in Y.
   */
  void SetTranslate(float tx, float ty);

  /**
   * Sets the transform type to NS_TRANSFORM_SCALE, with parameters sx and sy defining the scale amounts.
   *
   * @param sx The scale factor in X.
   * @param sy The scale factor in Y.
   */
  void SetScale(float sx, float sy);

  /**
   * Sets the transform type to NS_TRANSFORM_ROTATE, with parameter angle defining the rotation angle and
   * parameters cx and cy defining the optional centre of rotation.
   *
   * @param angle The rotation angle.
   * @param cx The x coordinate of centre of rotation.
   * @param cy The y coordinate of centre of rotation.
   */
  void SetRotate(float angle, float cx, float cy);

  /**
   * Sets the transform type to NS_TRANSFORM_SKEWX, with parameter angle defining the amount of skew.
   *
   * @param angle The skew angle.
   */
  void SetSkewX(float angle);

  /**
   * Sets the transform type to NS_TRANSFORM_SKEWY, with parameter angle defining the amount of skew.
   *
   * @param angle The skew angle.
   */
  void SetSkewY(float angle);
};

