/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef _FIELD_H_
#define _FIELD_H_

#include "JavaObject.h"

struct classOrInterface;

struct FieldOrMethod : JavaObject {
public:
  ClassOrInterface &getDeclaringClass() const;
  const char *getName() const; 
  int getModifiers() const;  // Access Flags

  ClassOrInterface &getType() const;
  virtual bool equals(JavaObject &obj) const;
  
  // int hashCode();

  // Returns the fully qualified name and type of the field or method, eg,
  // public static java.lang.thread.priority
  const char *toString() const;
};

struct Field : FieldOrMethod {
public:
  virtual bool equals(JavaObject &obj) const;

  // Get the value of the field
  JavaObject &get(JavaObject &obj);
  
  // Get the value of the field, converted to boolean
  boolean getBoolean(JavaObject &obj);

  // Get the value of the field in various forms. Appropriate exceptions
  // are tossed if obj is not of the right type
  uint8 getByte(JavaObject &obj);
  char getChar(JavaObject &obj);
  int16 getShort(JavaObject &obj);
  int32 getInt(JavaObject &obj);
  int64 getLong(JavaObject &obj);
  Flt32 getFloat(JavaObject &obj);
  Flt64 getDouble(JavaObject &obj);

  // Set the value of the field
  void set(JavaObject &obj, JavaObject &value);

  // Set the value of the field in various forms. Appropriate exceptions
  // are thrown if types don't match
  void setBoolean(JavaObject &obj, bool value);
  void setByte(JavaObject &obj, uint8 value);
  void setChar(JavaObject &obj, char value);
  void setShort(JavaObject &obj, int16 value);
  void setInt(JavaObject &obj, int32 value);
  void setLong(JavaObject &obj, int64 value);
  void setFloat(JavaObject &obj, Flt32 value);
  void setDouble(JavaObject &obj, Flt64 value);
  
};

struct Method : public FieldOrMethod {
  virtual bool equals(JavaObject &obj) const;

  ClassOrInterface &getReturnType();

  // Returns number of params
  int getParameterTypes(ClassOrInterface *&params);

  // Returns number of exception types
  int getExceptionTypes(ClassOrInterface *&params);
  
  // Invoke the specified method on the given object with specified
  // parameters. Throws gobs and gobs of exceptions if thwarted.
  JavaObject &invoke(JavaObject &obj, JavaObject *&args);

  
};


#endif /* _FIELD__H_ */





