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
#include "ClassFileSummary.h"
#include "ClassCentral.h"
#include "ErrorHandling.h"
#include "JavaObject.h"
#include "FieldOrMethod.h"

#include "plstr.h"
#include "prprf.h"
#include "LogModule.h"

UT_DEFINE_LOG_MODULE(ClassFileSummary);

/* symbolic V-Table node */
struct VTableNode {
  Method *method;
  ClassFileSummary *summary;  /* If method is NULL, summary points to defining class.
                                 This is used for instanceof() calculations. */
};

/* Annotation information for each item in the constant pool */
struct _AnnotationItem {
  bool resolved;   /* Has it been resolved? */
    

  /* Resolved information about field/method */
  union {
    const MethodInfo *m;   /* For method refs/interface method refs, 
							* name-and-type 
							*/    
    const FieldInfo *f;    /* For field refs, name and type */
    const InfoItem *item;  /* Holds MethodInfo/FieldInfo  */
  };
  const FieldOrMethod *descriptor;
  
  ClassFileSummary *info; /* Info for class referenced by this entry */
  bool isInit, isClinit;  /* True if method is <init> or <clinit> respectively */
};

/* Run the static initializers of this class if necessary */
inline void ClassFileSummary::runStatics(ClassOrInterface *declaringClass)
{
  if (!declaringClass->isArray() && !declaringClass->isInterface()) {
	Class *clz = static_cast<Class *> (declaringClass);
	
	if (!clz->isInitialized())
	  clz->runStaticInitializers();
  }
}

/* Return the UTF8 name of the superclass or nil if there is no superclass.
 */
const char *ClassFileSummary::getSuperclassName() const
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);
  
  ConstantClass *superClass = reader->getSuperClass();
  return superClass ? superClass->getUtf()->getUtfString() : 0;
}

/* returns the minor version of this class from the class file */
Uint16 ClassFileSummary::getMinorVersion() const 
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return reader->getMinorVersion();
}

/* Returns the major version of this class from the class file */
Uint16 ClassFileSummary::getMajorVersion() const 
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);
  
  return reader->getMajorVersion();
}

/* returns the access flags for this class from the class file */
Uint16 ClassFileSummary::getAccessFlags() const
{
    if (arrayType)
        verifyError(VerifyError::illegalAccess);
    
    return reader->getAccessFlags();
}


/* A ClassFileSummary object generates all static and runtime information
 * about a class, resolving references to other classes as neccessary.
 * staticPool is a pool which this class will use to allocate objects not 
 * directly used by the runtime.
 * runtimePool is used by this class to create runtime objects. 
 * strPool is a string pool used to intern strings encountered during
 * class loading. 
 * central is the repository that holds all loaded classes.
 * cName is the fully qualified name of the class. 
 * FileReader is an object that can read the class file.
 * If arrayType is true, indicates this class is an array class.
 * If arrayType is true, numDimensions indicates the number of dimensions
 * of the array.
 * If arrayType is true and primitiveElementType is true, then this
 * class is an array class of primitive types whose type is tk.
 * If arrayType is true and primitiveElementType is false, then this
 * class is an array class of elements with non-primitive type whose summary is
 * is elementSummary.
 */
ClassFileSummary::ClassFileSummary(Pool &staticPool, Pool &runtimePool,
								   StringPool &strPool, 
								   ClassCentral &central,
								   ClassWorld &world,
								   const char *cName, 
								   FileReader *fileReader,
								   bool arrayType,
								   Uint32 numDimensions,
								   bool primitiveElementType,
								   TypeKind tk,
								   ClassFileSummary *elementSummary) :
  staticPool(staticPool), runtimePool(runtimePool), 
  sp(strPool), central(central), world(world),
  parentInfo(0), clazz(0),
  fields(runtimePool), publicFields(runtimePool),
  methods(runtimePool), declaredMethods(runtimePool),
  publicMethods(runtimePool),
  constructors(runtimePool), publicConstructors(runtimePool),
  interfaces(runtimePool),
  arrayType(arrayType), numDimensions(numDimensions),
  elementSummary(elementSummary), 
  primitiveElementType(primitiveElementType),
  tk(tk), vIndicesUsed(runtimePool), subClasses(10)
{
  initp = sp.intern("<init>");
  clinitp = sp.intern("<clinit>");

  if (!arrayType) {
	assert(fileReader);
	reader =  new (staticPool) ClassFileReader(staticPool, sp, *fileReader);
	className = reader->getThisClass()->getUtf()->getUtfString();
  } else {
	nConstants = 0;
	annotations = 0;
	reader = 0;
	className = sp.intern(cName);
  }

  if (arrayType)
	return;
  
  constantPool = reader->getConstantPool();
  nConstants = reader->getConstantPoolCount();

  annotations = new (staticPool) AnnotationItem[nConstants];
  memset(annotations, 0, nConstants*sizeof(AnnotationItem));

  /* Set the sizes of fields, methods and interfaces */
  fields.setSize(reader->getFieldCount());

  methods.setSize(reader->getMethodCount());
  declaredMethods.setSize(reader->getDeclaredMethodCount());
  interfaces.setSize(reader->getInterfaceCount());
  constructors.setSize(reader->getConstructorCount());
  publicConstructors.setSize(reader->getPublicConstructorCount());
  
  fieldOffsets = new Uint32[fields.getSize()];
  Uint32 i;
  for (i = 0; i < fields.getSize(); i++) 
	fieldOffsets[i] = 0;
}


/* Update a class with parent information about its parent. setParent()
 * is called by ClassCentral after it has loaded and has information 
 * about the parent classes of this class. 
 */
void ClassFileSummary::setParent(ClassFileSummary *info) 
{
  getPackageNames();
  parentInfo = info;

  if (arrayType) {
      makeClazz();
      return;
  }
  
  Int32 parentPublicFieldCount = 0, parentPublicMethodCount = 0;

  if (parentInfo) {
	parentSize = info->getObjSize();      
	parentPublicFieldCount = parentInfo->getPublicFieldCount();
	parentPublicMethodCount = parentInfo->getPublicMethodCount();
	parentInfo->addSubclass(*this);
  } else
	parentSize = sizeof(JavaObject);  

  computeSize();  
  buildVTable();
  makeClazz();

  publicFields.setSize(reader->getPublicFieldCount() +
					   parentPublicFieldCount);
  publicMethods.setSize(reader->getPublicMethodCount() + 
						parentPublicMethodCount);
  
}


/* Build the Field class for a field located at index fieldIndex in the 
 * fields array of the class. fields.set() must have been called
 * with the correct field count before this routine is
 * called. This routine uses ClassCentral::addClass to resolve all
 * classes and interfaces that it encounters when parsing the field
 * descriptor.
 */
Field &ClassFileSummary::buildField(Uint32 fieldIndex)
{
  Field *field;
    
  if ((field = fields.get(fieldIndex)) != 0)
	return *field;

  const FieldInfo **fieldInfos = reader->getFieldInfo();
  const FieldInfo *fieldInfo = fieldInfos[fieldIndex];

  field = new (runtimePool) Field(packageName, unqualName,
								  fieldInfo->getName()->getUtfString(),
								  *this,
								  central,
								  runtimePool,
								  fieldOffsets[fieldIndex],
								  *fieldInfo);


  /* If this is a static primitive/string field, look for a ConstantValue attribute that will
   * set the value of this field
   */
  if (field->getModifiers() & CR_FIELD_STATIC) {
	  /* We will not resolve the type here completely since that will lead to
	   * circularities that we do not wish to deal with. Instead, examine the
	   * signature to determine the type
	   */
	  const char *sigString = fieldInfo->getDescriptor()->getUtfString();

	  if (Field::primitiveOrStringSignature(sigString)) {
		  const AttributeInfoItem *attribute = fieldInfo->getAttribute(CR_ATTRIBUTE_CONSTANTVALUE);
		  if (attribute) {
			  const AttributeConstantValue *cval = static_cast<const AttributeConstantValue *>(attribute);
			  ConstantPoolIndex cpi = cval->getConstantPoolIndex();
			  
			  ValueKind vk;
			  Value value;
			  
			  if (!(reader->lookupConstant(cpi, vk, value))) 
				  verifyError(VerifyError::badClassFormat);
			  
			  field->setFromValue(vk, value);
		  }				  		  
	  }
  }

  fields.set(fieldIndex, field);
  return *field;
}

/* Build the Method class for a field located at index methodIndex in the 
 * methods array of the class. methods.set() must have been called
 * with the correct method count before this routine is
 * called. This routine uses ClassCentral::addClass to resolve all
 * classes and interfaces that it encounters when parsing the method
 * descriptor.
 */ 
Method &ClassFileSummary::buildMethod(Uint32 methodIndex) 
{
  Method *method;

  assert(methodIndex < methods.getSize());

  const MethodInfo **methodInfos = reader->getMethodInfo();
  const MethodInfo *methodInfo = methodInfos[methodIndex];

  if (methodInfo->getName()->getUtfString() == initp)
	method = new (runtimePool) Constructor(packageName, unqualName,
										   methodInfo->getName()->getUtfString(),
										   *this,
										   central,
										   runtimePool,
										   *methodInfo);
  else 
	method = new (runtimePool) Method(packageName, unqualName,
									  methodInfo->getName()->getUtfString(),
									  *this,
									  central,
									  runtimePool,
									  *methodInfo);
  
  methods.set(methodIndex, method);
  world.addMethod(method->toString(), method); /* necessary for debugger right now, don't we have this information somewhere else */

  return *method;
}


/* Build the Interface class for the interface located at index 
 * interfaceIndex in the interfaces array of the class. interfaces.set()
 * must have been called with the correct interface count before this 
 * routine is called. This routine uses ClassCentral::addClass to resolve all
 * classes and interfaces that it encounters when parsing the method
 * descriptor.
 */ 
Interface &ClassFileSummary::buildInterface(Uint32 interfaceIndex)
{
  Interface *iface;

  assert(interfaceIndex < interfaces.getSize());

  if ((iface = interfaces.get(interfaceIndex)) != 0)
	return *iface;

  const ConstantPool *interfaceInfo = reader->getInterfaceInfo();
  const ConstantPoolItem *item = interfaceInfo->get(interfaceIndex);
  const ConstantClass *inf = static_cast<const ConstantClass *>(item);

  ClassFileSummary &info = central.addClass(inf->getUtf()->getUtfString());
	
  if (!(info.getAccessFlags() & CR_ACC_INTERFACE)) {
	UT_LOG(ClassFileSummary, PR_LOG_DEBUG, ("class %s: Cannot resolve or not an interface", 
		   inf->getUtf()->getUtfString()));
	verifyError(VerifyError::noClassDefFound);
  }
	
  iface = static_cast<Interface *>(info.getThisClass());
  interfaces.set(interfaceIndex, iface);
  return *iface;
}

/* Build a public field  located at given index in the publicFields
 * array of the class. Since this points to an entry in the Fields
 * array of the class, it will build this entry if it doesn't already
 * exist. The order in which we build the fields is as follows:
 * fields declared in superclasses of this class, fields declared in
 * interfaces implemented by this class, fields declared in the current class
 */
Field &ClassFileSummary::buildPublicField(Uint32 index)
{
	assert(index < publicFields.getSize());
	Uint32 parentCount = (parentInfo) ? parentInfo->getPublicFieldCount() : 0;

#if 0 
	Uint32 fieldCount = reader->getPublicFieldCount();
	
	Field *field;
	if (index < fieldCount) {   /* Field declared in this class */
		const Uint16 *publicFieldIndices = reader->getPublicFields();
		Uint32 actualIndex = publicFieldIndices[index];
		if (!(field = fields.get(actualIndex)))
			field = &buildField(actualIndex);
	} else if (index < (fieldCount+parentCount))
		field = &parentInfo->buildPublicField(index-fieldCount);
	else {
		assert(0);
	}
	
	publicFields.set(index, field);
	return *field;
  

#else
	Field *field;
	if (parentInfo && (index < parentCount)) {
		field = &parentInfo->buildPublicField(index);
		publicFields.set(index, field);
		return *field;
	}

	const Uint16 *publicFieldIndices = reader->getPublicFields();
	Uint32 actualIndex = publicFieldIndices[index-parentCount];
	if (!(field = fields.get(actualIndex)))
		field = &buildField(actualIndex);

	publicFields.set(index, field);
	return *field;
#endif
}

/* Build a public method located at given index in the publicMethods
 * array of the class. Since this points to an entry in the Methods
 * array of the class, it will build this entry if it doesn't already
 * exist.
 */
Method &ClassFileSummary::buildPublicMethod(Uint32 index)
{
  assert(index < publicMethods.getSize());

  Uint32 parentCount = (parentInfo) ? parentInfo->getPublicMethodCount() : 0;
  Method *method;
  if (parentInfo && (index < parentCount)) {
	method = &parentInfo->buildPublicMethod(index);
	publicMethods.set(index, method);
	return *method;
  }

  const Uint16 *publicMethodIndices = reader->getPublicMethods();
  Uint32 actualIndex = publicMethodIndices[index-parentCount];

  if (!(method = methods.get(actualIndex)))
	method = &buildMethod(actualIndex);

  publicMethods.set(index, method);
  return *method;
}


/* Build a declared method located at given index in the declaredMethods
 * array of the class. Since this points to an entry in the Methods
 * array of the class, it will build this entry if it doesn't already
 * exist.
 */
Method &ClassFileSummary::buildDeclaredMethod(Uint32 index)
{
  assert(index < publicMethods.getSize());

  const Uint16 *declaredMethodIndices = reader->getDeclaredMethods();
  Uint32 actualIndex = declaredMethodIndices[index];
  
  Method *method;
  if (!(method = methods.get(actualIndex)))
	method = &buildMethod(actualIndex);
  
  declaredMethods.set(index, method);
  return *method;
}

/* Build a constructor located at given index in the constructors
 * array of the class. 
 */
Constructor &ClassFileSummary::buildConstructor(Uint32 index)
{
  assert(index < constructors.getSize());

  Constructor *cons;

  const Uint16 *constructorIndices = reader->getConstructors();
  Uint32 actualIndex = constructorIndices[index];
  Method *method;
  if (!(method = methods.get(actualIndex)))
	method = &buildMethod(actualIndex);
  
  cons = static_cast<Constructor *> (method);
  constructors.set(index, cons);
  return *cons;
}

/* Build a public constructor located at given index in the
 * publicConstructors array of the class.
 */
Constructor &ClassFileSummary::buildPublicConstructor(Uint32 index)
{
  assert(index < publicConstructors.getSize());
  Uint32 parentCount = 0;
  Constructor *cons;

  const Uint16 *publicConstructorIndices = reader->getPublicConstructors();
  Uint32 actualIndex = publicConstructorIndices[index-parentCount];

  Method *method;
  if (!(method = methods.get(actualIndex)))
	method = &buildMethod(actualIndex);

  cons = static_cast<Constructor *> (method);
  publicConstructors.set(index, cons);
  return *cons;
}


/* Build Field objects for all fields in the class. fields.set() must
 * have been set with the correct field count before this routine is called. 
 * This routine uses ClassCentral::addClass 
 * to resolve all classes and interfaces that it encounters when parsing 
 * the field descriptor.
 */ 
void ClassFileSummary::buildFields() 
{
  for (Uint32 i = 0; i < fields.getSize(); i++)
	if (!fields.get(i))
	  buildField(i);
}


/* Build Method objects for all methods in the class. methods.set() must
 * have been set with the method count before this routine is called.
 * This routine resolves all classes and interfaces that it encounters when 
 * parsing the method descriptor.
 */ 
void ClassFileSummary::buildMethods() 
{
  for (Uint32 i = 0; i < methods.getSize(); i++)
	if (!methods.get(i))
	  buildMethod(i);
}



/* Build Interface objects for all interfaces in the class. interfaces.set()
 * must have been called with the correct interface count
 * before this routine is called. This routine uses 
 * ClassCentral::addClass to load the interface classes.
 */ 
void ClassFileSummary::buildInterfaces()
{
  for (Uint32 i = 0; i < interfaces.getSize(); i++)
	if (!interfaces.get(i))
	  buildInterface(i);
}

/* Build Field objects for all public fields in the class. */ 
void ClassFileSummary::buildPublicFields()
{
  for (Uint32 i = 0; i < publicFields.getSize(); i++)
	if (!publicFields.get(i))
	  buildPublicField(i);
}


/* Build Method objects for all public methods in the class. */ 
void ClassFileSummary::buildPublicMethods()
{
  for (Uint32 i = 0; i < publicMethods.getSize(); i++)
	if (!publicMethods.get(i))
	  buildPublicMethod(i);
}


/* Build Method objects for all declared methods in the class. */ 
void ClassFileSummary::buildDeclaredMethods()
{
  for (Uint32 i = 0; i < declaredMethods.getSize(); i++)
	if (!declaredMethods.get(i))
	  buildDeclaredMethod(i);
}

/* Build Constructor objects for all constructors in the class. */ 
void ClassFileSummary::buildConstructors()
{
  for (Uint32 i = 0; i < constructors.getSize(); i++)
	if (!constructors.get(i))
	  buildConstructor(i);
}

/* Build Constructor objects for all public constructors in the class. */ 
void ClassFileSummary::buildPublicConstructors()
{
  for (Uint32 i = 0; i < publicConstructors.getSize(); i++)
	if (!publicConstructors.get(i))
	  buildPublicConstructor(i);
}


/* return the field object corresponding to the given name
 * in the public fields of the class. Return NULL if 
 * there is no field with the given name. If interned is true,
 * then _name is interned. If publik is true, then look in public
 * fields only; otherwise, look in all fields.
 */
Field *ClassFileSummary::getField(const char *_name, bool publik, 
								  bool interned)
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  const char *name = (interned) ? _name : sp.get(_name);

  if (!name)
	return 0;

  const FieldInfo **fieldInfos = reader->getFieldInfo();
  for (Uint32 i = 0; i < fields.getSize(); i++) {
	const FieldInfo *fieldInfo = fieldInfos[i];

	if (publik && !(fieldInfo->getAccessFlags() & CR_FIELD_PUBLIC))
	  continue;

	if (name == fieldInfo->getName()->getUtfString())
	  return (fields.get(i)) ? fields.get(i) : &buildField(i);
  }

  /* If we're here, the field is not a declared field in this class.
   * Try the parent class if we're looking for a public field.
   */
  if (publik)
	return (parentInfo) ? parentInfo->getField(name, true, true) : 0;
  else
	return 0;
}

/* return the field object corresponding to the given name
 * in the public fields of the class. Return NULL if 
 * there is no field with the given name.
 */
Field *ClassFileSummary::getField(const char *name)
{
  return getField(name, true, false);
}


/* return the field object corresponding to the given name
 * in the fields of the class (including protected and private
 * fields). Return NULL if there is no field with the given name and type.
 */
Field *ClassFileSummary::getDeclaredField(const char *name)
{
  return getField(name, false, false);
}

/* return the Method object corresponding to the given name and
 * parameter types in the public methods of the class. Return NULL if 
 * there is no method with the given name and signature. If publik is true,
 * look in public methods only; otherwise, look in all declared methods.
 * If interned is true, then _name is an interned string. 
 * Notes: This function ends up resolving all candidate methods with
 * the matching name in order to generate their signatures, adding classes
 * as necessary.
 */
Method *ClassFileSummary::getMethod(const char *_name, 
									const Type *parameterTypes[],
									Int32 numParameterTypes,
									bool publik, bool interned)
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  const char *name = (interned) ? _name : sp.get(_name);

  if (!name)
	return 0;
  
  const MethodInfo **minfos = reader->getMethodInfo();
  for (Uint32 i = 0; i < methods.getSize(); i++) {
	const MethodInfo *minfo = minfos[i];
	Uint32 modifiers = minfo->getAccessFlags();

	if (publik && !(modifiers & CR_METHOD_PUBLIC))
	  continue;

	if (minfo->getName()->getUtfString() == name) {
	  Method *m;

	  if (!(m = methods.get(i)))
		m = &buildMethod(i);

	  const Signature &sig = m->getSignature();

	  /* The number of arguments is one higher than actual in the signature
	   * of instance methods to account for the self argument
	   */
	  Uint32 start;
	  Uint32 nArgs;

	  if (modifiers & CR_METHOD_STATIC) {
		nArgs = numParameterTypes;
		start = 0;
	  } else {
		nArgs = numParameterTypes+1;
		start = 1;
	  }

	  if (nArgs == sig.nArguments) {
		Uint32 j;
		for (j = start; j < sig.nArguments; j++)
		  if (sig.argumentTypes[j] != parameterTypes[j-start])
			break;

		if (j == sig.nArguments)   /* We have a match! */
		  return m;
	  }
	}
  }
  
  /* If we're here, we couldn't find a match in our methods. Try in
   * our superclass' methods if we're looking for a public method.
   */
  if (publik && (name != initp))
	return (parentInfo) ? parentInfo->getMethod(name, parameterTypes,
												numParameterTypes, true,
												true) : 0;
  else
	return 0;
}


/* return the Method object corresponding to the given name and
 * parameter types in the public methods of the class. Return NULL if 
 * there is no method with the given name and signature.
 */
Method *ClassFileSummary::getMethod(const char *name, 
									const Type *parameterTypes[],
									Int32 numParameterTypes)
{
  return getMethod(name, parameterTypes, numParameterTypes, true, false);
}


/* Return the method with given name and signature. Looks only in the
 * methods defined in this class (and not in methods this class might have 
 * inherited from a parent class). Both name and sig are assumed to be 
 * interned strings; however, sig can be nil if the method is not overloaded.
 * Returns NULL on failure.
 */
Method *ClassFileSummary::getMethod(const char *name, const char *sig)
{
  if (arrayType)
	return 0;

  Uint16 methodIndex;
  const MethodInfo *minfo;
  minfo = reader->lookupMethod(name, sig, methodIndex);
  if (!minfo)
	return NULL;

  return &getMethod(methodIndex);
}

/* return the method object corresponding to the given name and
 * parameter types in the methods of the class (including protected and 
 * private methods). Return NULL if there is no method with the given name 
 * and signature.
 */
Method *ClassFileSummary::getDeclaredMethod(const char *name, 
											const Type *parameterTypes[],
											Int32 numParameterTypes)
{
  return getMethod(name, parameterTypes, numParameterTypes, false, false);
}


/* return the Constructor object corresponding to the given name
 * and parameter types on the methods of the class (including protected
 * and private methods). Return NULL if there is no method with
 * the given name and signature.
 */
Constructor *ClassFileSummary::getConstructor(const Type *parameterTypes[],
											  Int32 numParameterTypes)
{
  return static_cast<Constructor *>(getMethod(initp, parameterTypes,
											  numParameterTypes, true,
											  true));
}


/* return the public Constructor object corresponding to the given name
 * and parameter types on the methods of the class (including protected
 * and private methods). Return NULL if there is no method with
 * the given name and signature.
 */
Constructor *ClassFileSummary::getDeclaredConstructor(const Type *parameterTypes[],
													  Int32 numParameterTypes)
{
  return static_cast<Constructor *>(getMethod(initp, parameterTypes,
											  numParameterTypes, false,
											  true));
}


/* If a field/method/interface method reference with the given name and 
 * signature is present of the field/method array of the class, then 
 * return the corresponding Field/Method descriptor; also return 
 * compile-time information about the field/method via item. Otherwise, 
 * return NULL. 'type' indicates whether to look for a field, method or 
 * interface method; valid values for type are CR_CONSTANT_FIELDREF,
 * CR_CONSTANT_METHODREF, CR_CONSTANT_INTERFACEMETHODREF.
 * resolveName only returns non-NULL for fields and methods that are
 * actually present in the class (as opposed to being inherited without
 * being overridden).
 */
const FieldOrMethod *ClassFileSummary::resolveName(const Uint8 type, 
												   const char *name,
												   const char *desc,
												   const InfoItem *&item)
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);


  Uint16 index;
  if (!(item = reader->lookupFieldOrMethod(type, name, desc, index)))
	return 0;

  if (type == CR_CONSTANT_FIELDREF) {
	if (!fields.get(index))
	  buildField(index);
	return fields.get(index);
  } else {
	if (!methods.get(index))
	  buildMethod(index);
	return methods.get(index);
  }
}

/* 
 * Annotate a constantPool entry at index cpi with given information.
 * Also check whether this class has access permissions to the item,
 * which is defined inside class "info".
 */
void ClassFileSummary::annotate(Uint8 type, 
								ConstantPoolIndex index, 
								const InfoItem *item,
								const FieldOrMethod *descriptor,
								ClassFileSummary *info,
								const char *name,
								const char * /*sig*/)
{
  if (item) {
	/* Check whether we have permissions to access this field or method */
	Uint32 modifiers = descriptor->getModifiers();

	if ((modifiers & CR_FIELD_PRIVATE) && info != this)
	  verifyError(VerifyError::illegalAccess);
	else if ((modifiers & CR_FIELD_PROTECTED)) {
	  /* We should be a subclass of the given class */

#if 0
	  /* For now this check is ignored since we need to check for more
	   * things than subclassing to see if access is allowed
	   */
	  if (!isSubClassOf(info))
		verifyError(VerifyError::illegalAccess);
#endif
	}

    annotations[index].item = item;
    annotations[index].info = info;
	annotations[index].descriptor = descriptor;

	if (type != CR_CONSTANT_FIELDREF) {
	  annotations[index].isInit = (name == initp);
	  annotations[index].isClinit = (name == clinitp);
	} 
      
    annotations[index].resolved = true;

  } else
	verifyError(VerifyError::noClassDefFound);

}

/*
 * Lookup a method with the given name and signature only in superclasses
 * of the current class; annotate the current class' annotation information
 * with the closest superclass with a match. type
 * indicates whether to look for a method (CR_CONSTANT_METHODREF) or
 * interface method (CR_CONSTANT_INTERFACEMETHODREF)
 */
void ClassFileSummary::resolveSpecial(const Uint8 type, 
									  ConstantPoolIndex index)
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);
  
  ConstantRef *ref = (ConstantRef *) constantPool->get(index);
  ConstantNameAndType *nt = ref->getTypeInfo();
  const char *name = nt->getName()->getUtfString();
  const char *sig = nt->getDesc()->getUtfString();
  const InfoItem *item;
  const FieldOrMethod *descriptor;

  ClassFileSummary *parent;
  for (parent = parentInfo; parent; parent = parent->getParentInfo()) {
	if ((descriptor = parent->resolveName(type, name, sig, item)) != 0)
	  break;	  
  }
  
  if (!parent)
	verifyError(VerifyError::badConstantPoolIndex);

  annotate(type, index, item, descriptor, parent, name, sig);
}


/* Resolve a field/method/interface present whose index in the 
 * ConstantPool of this class is given by "index". If successful, update
 * annotation information for this field. 
 */
void ClassFileSummary::resolveFieldOrMethod(const Uint8 type, 
											ConstantPoolIndex index)
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);
  
  ConstantRef *ref = (ConstantRef *) constantPool->get(index);
  
  if (ref->getType() != type)
	verifyError(VerifyError::badClassFormat);

  ConstantClass *classInfo = ref->getClassInfo();
  ClassFileSummary &info = central.addClass(classInfo->getUtf()->getUtfString());
  
  ConstantNameAndType *nt = ref->getTypeInfo();
  const char *name = nt->getName()->getUtfString();
  const char *sig = nt->getDesc()->getUtfString();

  const InfoItem *item;
  const FieldOrMethod *descriptor = info.resolveName(type, name, sig, item);
  
  annotate(type, index, item, descriptor, &info, name, sig);
}

/* Break up the fully qualified name of this class into a packageName
 * and an unqualified class name.
 */
void ClassFileSummary::getPackageNames()
{
  const char *s = &className[PL_strlen(className)-1];
  char *t;

  while (s != className && *s != '/')
    s--;

  const char *mark = s;

  if (*s == '/')
    s++;

  t = new char[PL_strlen(s)+1];
  PL_strcpy(t, s);

  char *unqualNameTemp = t;

  int len = mark-className+1;
  char *packageNameTemp = new char[len];

  PL_strncpy((char *) packageNameTemp, className, len-1);
  packageNameTemp[len-1] = 0;

  /* Replace all slashes with dots in packageName */
  for (char *temp = packageNameTemp; *temp; temp++)
    if (*temp == '/') *temp = '.';

  /* Now intern these names and free the originals */
  packageName = sp.intern(packageNameTemp);
  unqualName = sp.intern(unqualNameTemp);

  delete [] packageNameTemp;
  delete [] unqualNameTemp;
}



/* Compute the size of the class. Note that we don't actually parse field 
 * descriptors or do any actual resolution here..
 * that is done when we actually resolve the field.
 * This function must not be called until the parent's size is available.
 * XXX This should be optimized eventually, since we generate padding bytes
 * for every char and short field.
 */
void ClassFileSummary::computeSize()
{
 totalSize = parentSize;

 if (arrayType) {
   /* parentSize is sizeof(Object), which is 4 */
   assert(parentSize == 4);
   return;
 }

  const FieldInfo **finfos = reader->getFieldInfo();
  Uint16 fieldCount = reader->getFieldCount();

  for (Uint16 i = 0; i < fieldCount; i++) {	
	const char *desc = finfos[i]->getDescriptor()->getUtfString();

	if ((finfos[i]->getAccessFlags() & CR_FIELD_STATIC))
	  continue;
	
	int size, alignment = 4; /* For everything except float and double */
	
	switch (*desc) {
	case 'B':
	case 'C':
	  size = 1;
	  break;
	  
	case 'J':
	case 'D':
	  size = 8;
	  alignment = 8;
	  break;
    
	case 'F':
	case 'I':
	  size = 4;
	  break;
    
	case 'S':
	  size = 2;
	  break;

	case 'Z':
	  size = 1;
	  break;

	case '[': 
	case 'L': 
	  size = 4;
	  break;

	default:
	  verifyError(VerifyError::badClassFormat);
	  
	}

	/* Make sure this field is aligned on the proper alignment */
	totalSize = ALIGN(totalSize, alignment);
	fieldOffsets[i] = totalSize;
	totalSize += size;
  }
}

/* An interface method was invoked for which there is no
   corresponding implementation.
 */
static void
throwIncompatibleClassChangeError()
{
    assert(0);
    /* XXX - We need to throw a Java error here, not a C++ one. */
    /* XXX - Should be CloneNotSupportedException if method is "clone" */
    verifyError(VerifyError::incompatibleClassChange);
}

/* Make the Class or Interface object representing the class. At this point,
 * we are actually making an incomplete class without the complete field/
 * method information; this gets patched up as we resolve fields and
 * methods.
 */
Type *ClassFileSummary::makeClazz() 
{
  Type *parentType = NULL;

  if (clazz)
    return clazz;
  
  /* If this class is already in the pool, it is not neccessarily an error
   * since it might have been put there by someone else.
   */
  if (world.getType(getClassName(), clazz)) {
    assert(0); /* fur - I don't think this can happen */
    return clazz;
  }
  
  /* Construction of array objects is handled specially */
  if (arrayType) {
    Type *elementClass;
    Type *arrayType;
    
    /* An array of non-primitive element type ? */
    if (elementSummary) {
      elementClass = elementSummary->getThisClass();	  
    } else {
      assert (primitiveElementType);
      elementClass = const_cast<Type *>(static_cast<const Type *>(&PrimitiveType::obtain(tk)));
    }
    
    /* Array components can themselves be arrays, but element types cannot. */
    assert(elementClass->typeKind != tkArray);

    /* Obtain the type of the array */
    arrayType = elementClass;
    for (Uint32 i = 0; i < numDimensions; i++)
      arrayType = const_cast<Type *>(static_cast<const Type *>(&arrayType->getArrayType()));

    return (clazz = arrayType);	
  } 
  
  if (parentInfo) {
    parentType = parentInfo->getThisClass();
	assert(parentType);
  }
  
  Package *pkg;
  pkg = new (runtimePool) Package(packageName);
  
  
  /* We will first build a Class object and resolve its fields lazily.
   */
  if (!(reader->getAccessFlags() & CR_ACC_INTERFACE)) {
	clazz = &Class::make(*pkg, unqualName, 
						 static_cast<Class *>(parentType), 
						 interfaces.getSize(), 
						 interfaces.getEntries(), this,
						 ((reader->getAccessFlags() & CR_ACC_FINAL) != 0),
						 totalSize, numVTableEntries,
                         interfaceDispatchTable->getVOffsetTableBaseAddress(),
                         interfaceDispatchTable->getAssignableTableBaseAddress());

	/* Construct the actual vtable used for runtime method dispatch from the
       symbolic vtable.  Each vtable entry points to a small stub that causes
       the corresponding method to be compiled and the vtable backpatched. */
	Class *thisClass = (static_cast<Class *>(clazz));
    void *classOrCodePtr;

#if 0
	if (!strcmp(getClassName(), "java/lang/reflect/Method"))
	  assert(0);
#endif

	for (Uint32 i = 0; i < numVTableEntries; i++) {
      Method *method = vtable[i].method;
      if (method) {
        classOrCodePtr = addressFunction(method->getCode(true));
      } else if (vtable[i].summary) {
        classOrCodePtr = vtable[i].summary->getThisClass();
      } else {
        /* We've encountered an interface method for which there is no
           corresponding implementation.  This case is specifically
           permitted by the JLS, so we don't throw a compile-time error.
           Rather, if this method is actually invoked, an
           IncompatibleClassChangeError is thrown. */
        classOrCodePtr = throwIncompatibleClassChangeError;
      }
      assert(classOrCodePtr);
      thisClass->setVTableEntry(i, classOrCodePtr);
    }

    /* Setup the Class object so that assignability tests can be done at runtime where
       the target type is either an interface or an array of interfaces. */
    thisClass->assignableMatchValue = interfaceDispatchTable->getAssignableMatchValue();
  } else	
	clazz = new (runtimePool) Interface(*pkg, unqualName, 
										interfaces.getSize(), 
										interfaces.getEntries(), this);
    
  world.addType(getClassName(), clazz);
  return clazz;
}


/*
 * Look up a constant in this class's constant pool. cpi is the index of 
 * this constant in the constant pool of the class.  Return the constant's
 * ValueKind and value.
 */
ValueKind ClassFileSummary::lookupConstant(ConstantPoolIndex cpi, 
										   Value &value) const
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  ValueKind vk;
  if (!(reader->lookupConstant(cpi, vk, value)))
	verifyError(VerifyError::badConstantPoolIndex);

  return vk;
}

/* Lookup a field whose index in the class' constant pool is cpi.
 * Return information about the class on success; throw a VerifyError
 * on failure. If this method sets isConstant to true, it guarantees that 
 * the field will never change value from its current value.
 */
const Field *ClassFileSummary::lookupField(ConstantPoolIndex cpi, 
										   ValueKind &vk, 
										   TypeKind &tk,
										   bool &isVolatile, 
										   bool &isConstant,
										   bool &isStatic)
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  if (constantPool->get(cpi)->getType() != CR_CONSTANT_FIELDREF)
	verifyError(VerifyError::badConstantPoolIndex);

  if (!annotations[cpi].resolved)
	resolveFieldOrMethod(CR_CONSTANT_FIELDREF, cpi);
    
  Uint32 modifiers = annotations[cpi].descriptor->getModifiers();

  isStatic = (modifiers & CR_FIELD_STATIC) != 0;
  
  const Field *field = static_cast<const Field *> 
	(annotations[cpi].descriptor);

  tk = field->getType().typeKind;
  vk = typeKindToValueKind(tk);

  isVolatile = (modifiers & CR_FIELD_VOLATILE) != 0;
  isConstant = false; // isConstant = (modifiers & CR_FIELD_CONSTANT) != 0;

  /* Run static initializers for this class if necessary */
  runStatics(field->getDeclaringClass());
  
  return field;
}


/*
 * Look up a static field whose index in the class's constant pool is
 * represented by cpi. Return the field's Java type, the corresponding 
 * ValueKind, the field's address in memory, and two flags indicating 
 * whether the field is volatile and whether it is a constant. If this 
 * method sets isConstant to true, it guarantees that 
 * the field will never change value from its current value 
 * (currently in memory at a).
 */
TypeKind ClassFileSummary::lookupStaticField(ConstantPoolIndex cpi, 
											 ValueKind &vk, addr &a,
											 bool &isVolatile, 
											 bool &isConstant)
{
  bool isStatic;

  TypeKind tk;
  const Field *field = lookupField(cpi, vk, tk, isVolatile, isConstant, 
								   isStatic);
  
  if (!isStatic)
	verifyError(VerifyError::noSuchField);

  a = field->getAddress();
  assert(addressFunction(a));
  return tk;
}


/*
 * Look up an instance field in this class's constant pool.  Return the 
 * field's Java type, the corresponding ValueKind, the field's offset from 
 * the beginning of an instance object, and two flags indicating whether the 
 * field is volatile and whether it is a constant.  If this method sets 
 * isConstant to true, it guarantees that reading the field multiple times 
 * from a given instance object will always return the same value (which 
 * may, however, be different for different instance objects).
 */
TypeKind ClassFileSummary::lookupInstanceField(ConstantPoolIndex cpi, 
											   ValueKind &vk, Uint32 &offset,
											   bool &isVolatile, 
											   bool &isConstant)
{  
  bool isStatic;

  TypeKind tk;
  const Field *field = lookupField(cpi, vk, tk, isVolatile, isConstant, 
								   isStatic);
  
  if (isStatic)
	verifyError(VerifyError::noSuchField);
  
  offset = field->getOffset();
  return tk;
  
}

/*
 * Look up a class or interface stored at index cpi in this class's constant 
 * pool. This method sets isInterface to true if the resolved item is an
 * interface, false if it is a class.
 */
ClassOrInterface &ClassFileSummary::lookupClassOrInterface(ConstantPoolIndex cpi, 
														   bool &isInterface)
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  /* Constant-pool index must be a CONSTANT_CLASS */
  if (constantPool->get(cpi)->getType() != CR_CONSTANT_CLASS)
    verifyError(VerifyError::badConstantPoolIndex);

  const char *className = ((ConstantClass *) constantPool->get(cpi))->getUtf()->getUtfString();

  ClassFileSummary &info = central.addClass(className);
  
  /* Array classes are never interfaces */
  if (*className == '[')
	isInterface = false;
  else
	isInterface = (info.getAccessFlags() & CR_ACC_INTERFACE) != 0;

  return *(static_cast<ClassOrInterface *>(info.getThisClass()));
}

/*
 * Look up a class stored at index cpi in this class's constant pool.
 * Ensure that the constant pool entry at index cpi is a CONSTANT_Class that
 * refers to a class (not an interface).
 */
Class &ClassFileSummary::lookupClass(ConstantPoolIndex cpi)
{
  bool isInterface;
  ClassOrInterface &clazz = lookupClassOrInterface(cpi, isInterface);
  
  if (isInterface)
	verifyError(VerifyError::badConstantPoolIndex);

  return *(static_cast<Class *>(&clazz));
}


/*
 * Look up an interface stored at index cpi in this class's constant pool.
 * Ensure that the constant pool entry at index cpi is a CONSTANT_Class that
 * refers to an interface (not a class).
 */
Interface &ClassFileSummary::lookupInterface(ConstantPoolIndex cpi)
{
  bool isInterface;
  ClassOrInterface &clazz = lookupClassOrInterface(cpi, isInterface);
  
  if (!isInterface)
	verifyError(VerifyError::badConstantPoolIndex);

  return *(static_cast<Interface *>(&clazz));
}


/*
 * Look up a class, interface, or array type stored at index cpi in this 
 * class's constant pool. Ensure that the constant pool entry at index cpi 
 * is a CONSTANT_Class.
 */
Type &ClassFileSummary::lookupType(ConstantPoolIndex cpi)
{
  bool isInterface;
  ClassOrInterface &clazz = lookupClassOrInterface(cpi, isInterface);

  return *(static_cast<Type *>(&clazz));
}


/* Update the vtable entry for a given method to reflect its resolved address
   in this class and all subclasses. */
void ClassFileSummary::updateVTable(Method &method)
{
  Vector<Uint32> *vIndices = NULL;
  vIndicesUsed.get(const_cast<const Method*>(&method), &vIndices);
  
  if (!vIndices)
    return;

  Uint32 numVIndices = vIndices->size();
  Uint32 i;
  for (i = 0; i < numVIndices; i++) {
    Uint32 vIndex = (*vIndices)[i];
    assert(vIndex < numVTableEntries);
    getThisClass()->setVTableEntry(vIndex, addressFunction(method.getCode(true)));
  }

  /* Update subclass vtables */
  Uint32 size = subClasses.size();
  for (i = 0; i < size; i++)
	subClasses[i]->updateVTable(method);
}

/*
 * Look up a method stored at index cpi in this class's constant pool.
 * Return the method's signature and whether or not it is static. If
 * special is true, the method is resolved as specified in the 
 * Java VM specification for invoke-special. If this function sets
 * dynamicallyResolved to true, it indicates that a vtable entry
 * exists for this function at index vIndex. Otherwise, this method
 * has been resolved statically; the address of the method is returned
 * through a. isStatic is set to true if the method is a static method.
 * (Note that non-static methods can also be statically resolved). 
 * isInit is set to true if this method is <init>; Likewise, isClInit
 * is set to true if this method is <clinit>.
 *
 * Return the Method object corresponding to the method if the method
 * has been resolved statically (this includes all special resolutions).
 * If the method must be resolved dynamically, return the Method object
 * corresponding to that method resolved for the class given by the
 * MethodRef or InterfaceMethodRef (really?) in the constant pool
 * at index cpi.
 */
const Method *ClassFileSummary::lookupMethod(Uint8 type,
											 ConstantPoolIndex cpi, 
											 Signature &sig, 
											 bool &isStatic,
											 bool &dynamicallyResolved, 
											 Uint32 &vIndex, 
											 addr &a,
											 bool &isInit,
											 bool &isClInit,
											 bool special)

{
  /* XXX Need to support array methods here */
  if (arrayType)
	verifyError(VerifyError::illegalAccess);
  
  if (constantPool->get(cpi)->getType() != type)
	verifyError(VerifyError::badConstantPoolIndex);
  
  if (!annotations[cpi].resolved) {
    resolveFieldOrMethod(type, cpi);
	isInit = annotations[cpi].isInit;
	isClInit = annotations[cpi].isClinit;
	
	const Method *method = static_cast<const Method *> 
	  (annotations[cpi].descriptor);
	Uint16 modifiers = method->getModifiers();

	/* Special methods may need to be resolved again */
	if (special) {
	  if (!isInit && !isClInit) {
		if (!(modifiers & CR_METHOD_PRIVATE) && 
			isSubClassOf(annotations[cpi].info) &&
			(reader->getAccessFlags() & CR_ACC_SUPER)) {
		  /* We must re-resolve the method, choosing the nearest superclass */
		  resolveSpecial(type, cpi);
		} 
	  }
	}
  }

  Method *method = const_cast<Method *> 
	(static_cast<const Method *>(annotations[cpi].descriptor));
  Uint16 modifiers = method->getModifiers();
  isInit = annotations[cpi].isInit;
  isClInit = annotations[cpi].isClinit;
  isStatic = (modifiers & CR_METHOD_STATIC) != 0;
  
  if (type == CR_CONSTANT_INTERFACEMETHODREF) {  
	if (!(modifiers & CR_METHOD_PUBLIC))
	  verifyError(VerifyError::badClassFormat);
  }

  dynamicallyResolved = method->getDynamicallyDispatched();
  if (dynamicallyResolved)
	method->getVIndex(vIndex);
  else {
	a = method->getCode();
    assert(addressFunction(a));
  }

  sig = *(const_cast<Signature *>(&method->getSignature()));
 
  return method;
}

					 
/*
 * Look up a static method stored at index cpi in this class's constant pool.
 * Return the method's signature and address and its Method object.
 */
const Method &ClassFileSummary::lookupStaticMethod(ConstantPoolIndex cpi, 
												   Signature &sig, addr &a)
{
  bool dynamicallyResolved, isStatic, isInit, isClInit;
  Uint32 vIndex;

  const Method *method = lookupMethod(CR_CONSTANT_METHODREF, cpi, sig, isStatic,
			   dynamicallyResolved, vIndex, a, isInit, isClInit, false);  

  if (!isStatic || isInit || isClInit)
	verifyError(VerifyError::incompatibleClassChange);

  assert(method);
  assert(addressFunction(a));
  return *method;
}


/*
 * Look up a virtual method stored at index cpi in this class's constant pool.
 * Return the method's signature and either the vtable index of this 
 * method's vtable entry or this method's address.  If lookupVirtualMethod returns 
 * nil, then the method should be called via the vtable using the given index.
 * If lookupVirtualMethod returns a Method object, then the method dispatch has been 
 * statically resolved (which can happen, for instance, for private methods 
 * and methods defined in final classes), and the method should be called 
 * directly using the given address and/or Method object.
 */
const Method *ClassFileSummary::lookupVirtualMethod(ConstantPoolIndex cpi, 
										   Signature &sig,
										   Uint32 &vIndex, 
										   addr &a)
{
  bool dynamicallyResolved, isStatic, isInit, isClInit;  
  
  const Method *method = lookupMethod(CR_CONSTANT_METHODREF, cpi, sig, isStatic,
			   dynamicallyResolved, vIndex, a, isInit, isClInit, false);  
  
  if (isStatic || isInit || isClInit)
	verifyError(VerifyError::incompatibleClassChange);

  if (dynamicallyResolved)
    return 0;
  assert(method);
  return method;
}


/*
 * Look up an interface method stored at index cpi in this class's constant 
 * pool. Return the method's signature and either the interface index of 
 * this method's entry or this method's address.  If lookupInterfaceMethod 
 * returns nil, then the method should be called via the interface table using the 
 * given index.  If lookupInterfaceMethod returns a Method object, then the method
 * dispatch has been statically resolved, and the method should be called directly
 * using the given address and/or Method object.
 *
 * nArgs is the number of arguments as provided in the invokeinterface bytecode;
 * it's redundant and provided only for verification purposes. If dynamic memory
 * needs to be allocated to hold the signature's arguments array, allocate that
 * memory from the given pool.
 */
const Method *ClassFileSummary::lookupInterfaceMethod(ConstantPoolIndex cpi, 
											 Signature &sig,
											 Uint32 &vIndex, 
                                             Uint32 &interfaceNumber,
											 addr &a, uint nArgs)
{
  bool dynamicallyResolved, isStatic, isInit, isClInit;  
  
  const Method *method = lookupMethod(CR_CONSTANT_INTERFACEMETHODREF, 
									 cpi, sig, isStatic,
									 dynamicallyResolved, 
									 vIndex, 
									 a, isInit, isClInit, 
									 false);

  if (isStatic || isInit || isClInit)
	verifyError(VerifyError::incompatibleClassChange);

  Uint32 modifiers = method->getModifiers();

  /* An interface method cannot be final */
  if ((modifiers & CR_METHOD_FINAL))
	verifyError(VerifyError::badClassFormat);
  
#ifndef TESTING
//  if (sig.nArguments != nArgs)
//	verifyError(VerifyError::badClassFormat);
#else
  /* Disabled for testing */
  nArgs = 0; /* Dummy */
#endif

  interfaceNumber = asInterface(*method->getDeclaringClass()).getInterfaceNumber();

  assert(dynamicallyResolved);
  return 0;
}


/*
 * Look up a special method stored at index cpi in this class's constant pool.
 * Return the method's signature, address, and Method object.  Follow the lookup
 * process for invokespecial as described in the Java VM specification.
 * Set isInit to true if this method is <init>.
 */
const Method &ClassFileSummary::lookupSpecialMethod(ConstantPoolIndex cpi,
										   Signature &sig, bool &isInit, addr &a)
{
  bool dynamicallyResolved, isStatic, isClInit;
  Uint32 vIndex;
  
  const Method *method = lookupMethod(CR_CONSTANT_METHODREF, cpi, sig, isStatic,
			   dynamicallyResolved, vIndex, a, isInit, isClInit, 
			   true);    

  if (isStatic || isClInit)
    verifyError(VerifyError::incompatibleClassChange);

  /* Special methods are always dispatched statically */
  a = const_cast<Method *>(method)->getCode();
  assert(addressFunction(a));

  assert(method);
  return *method;
}

void ClassFileSummary::addInterfaceToVTable(const Interface *superInterface, 
                                            VTableNode *vtable,
                                            Uint32 &numEntries)
{
  Uint32 firstVIndex = numEntries;
  ClassFileSummary &interfaceSummary = *superInterface->summary;

  /* Record the base index of this interface's vtable for purposes
     of interface method dispatch. */
  if (!(getAccessFlags() & CR_ACC_INTERFACE))
    interfaceDispatchTable->add(superInterface->getInterfaceNumber(),
                                vTableIndexToOffset(numEntries));

  /* Add the vtable entries for our subinterfaces */
  int numInterfaces = interfaceSummary.getInterfaceCount();
  const Interface **interfaces = interfaceSummary.getInterfaces();
  for (int j = 0; j < numInterfaces; j++)
     addInterfaceToVTable(interfaces[j], vtable, numEntries);

  Uint32 methodCount = interfaceSummary.reader->getMethodCount();
  const Method **methods = interfaceSummary.getMethods();
  for (Uint32 i = 0; i < methodCount; i++) {
    Method *method = const_cast<Method *>(methods[i]);
    const char *name = method->getName();
    const char *signature = method->getSignatureString(); 
    
    /* No Vtable entries for <init> and <clinit> */
    if (name == initp) { /* An interface cannot have a constructor */
		assert(0);
        continue;
    }

	if (name == clinitp) {
      /* It is possible to have static initializers in an interface
	   * that initialize static final fields in the interface
	   */
      continue;
    }
     
    if (getAccessFlags() & CR_ACC_INTERFACE) {
      vtable[numEntries].method = method;
      method->setVIndex(numEntries - firstVIndex);
      numEntries++;
      continue;
    }

    ClassFileSummary *cfs = this;
    while (1) {
      Uint16 methodIndex;
      const MethodInfo *minfo;

      minfo = cfs->reader->lookupMethod(name, signature, methodIndex);
      if (minfo) {
        const Method **implementedMethods = cfs->getMethods();
        method = const_cast<Method*>(implementedMethods[methodIndex]);
        method->setVIndex(numEntries);
        addVIndexForMethod(method, numEntries);
        vtable[numEntries].method = method;
        break;
      }
      cfs = cfs->parentInfo;
      if (!cfs) {
        /* We've encountered an interface method for which there is no
           corresponding implementation.  This case is specifically
           permitted by the JLS, so we don't throw a compile-time error.
           Rather, if this method is actually invoked, an
           IncompatibleClassChangeError is thrown. */
        break;
      }
    }
    numEntries++;
  }
}


/* Build the vtable for this class. This function must never be called
 * before setParent() is called, since it needs the parent class vtable
 * information.  This function also builds the table used to perform
 * constant-time interface method lookups at run-time.
 *
 * Each vtable lists entries in this order:
 *   1) A copy of the vtable of the superclasses, containing all the
 *      inherited methods (except for those methods which are overridden by
 *      this class, which are replaced in the vtable)
 *   2) The methods of all interfaces not inherited from superclasses, i.e. methods
 *      of the direct superinterfaces, including the superinterfaces of the direct
 *      superinterfaces.
 *   3) All other virtual methods defined by this class
 *   4) A single vtable entry that points to the Class or Interface that defines
 *      the vtable.  This entry is used to perform instanceof() operations in
 *      constant time.
 * 
 * It is possible for a method to be listed more than once in a single vtable.
 * For example, the same method may appear in more than one interface that this
 * class implements.  Another possibility is that an inherited method is used to
 * implement a superinterface of this class.  Because a method can have more than
 * one vtable index, the smallest one is stored as the canonical vIndex to be used
 * for ordinary (non-interface) method lookup.
 */
void ClassFileSummary::buildVTable()
{
    /* XXX Need to do the right thing for array classes here */
    assert(!arrayType);
    
    VTableNode *vt;
    Uint16 methodIndex;
    Uint32 nvt;
    Uint32 vIndex;
    
    if (arrayType)
        return;
 
    interfaceDispatchTable = new InterfaceDispatchTable();

    if (!parentInfo) {
        nvt = 0;
    } else {
        nvt = parentInfo->getVTable(vt);
        interfaceDispatchTable->inherit(parentInfo->interfaceDispatchTable);
    }
    

    const Method **methods = getMethods();
    int methodCount = reader->getMethodCount();
    
    /* Compute the size of the vtables for interfaces which get embedded in this class' vtable */
    int numInterfaces = getInterfaceCount();
    const Interface **interfaces = getInterfaces();
    Uint32 interfaceVtablesTotalSize = 0;
    int j;
    for (j = 0; j < numInterfaces; j++) {
        interfaceVtablesTotalSize += interfaces[j]->summary->numVTableEntries;
    }
    
    /* The maximum possible number of entries is nvt + methodCount + interfaceVtablesSize + 1.
     * The last vtable entry is used as a self-referential pointer to the class for performing
     * the instanceof() operation in constant time.  We may allocate more memory than
     * we actually need to hold the vtable, because some of this class' methods override
     * parent methods which already occupy vtable slots.
     */
    Int32 numVTableElements = nvt + methodCount + interfaceVtablesTotalSize + 1;
    vtable = new (staticPool) VTableNode[numVTableElements];
    memset(vtable, 0, sizeof(VTableNode)*numVTableElements);
    Uint32 numEntries = 0;
    
    /* Copy the parent's vtable */
    for (;numEntries < nvt; numEntries++) {
        vtable[numEntries] = vt[numEntries];
        Method *method = vt[numEntries].method;
        
        /* This might be the self-referential class pointer vtable entry of some
           super-class (used for implementing instanceof), rather than a method */
        if (!method) {
            assert(vt[numEntries].summary);
            continue;
        }
        
        /* See if the method is overridden by this class. */
        const char *name = method->getName();
        const char *signature = method->getSignatureString();
        const MethodInfo *minfo = reader->lookupMethod(name, signature, methodIndex);
        if (minfo) {
            method = const_cast<Method *>(methods[methodIndex]);
            method->setVIndex(numEntries);
            vtable[numEntries].method = method;
        }
        
        /* It's not permitted to have an abstract method in a non-abstract class.
           (If this method is invoked, an error will be thrown, so it is not necessary
           to throw an error here.) */
        assert(! ( (method->getModifiers() & CR_METHOD_ABSTRACT) &&
            !(getAccessFlags() & CR_ACC_ABSTRACT) ));
        addVIndexForMethod(method, numEntries);
    }
    
    /* Add the vtable entries for interfaces.  It is possible that a method may
       be added to the vtable for an interface even though it can be statically
       resolved during normal method dispatch.  That is, invoking a virtual
       method on an interface variable may require a method lookup, even though
       no lookup is required when the corresponding method is invoked on
       the same exact object stored in a non-interface variable type.
    */
    for (j = 0; j < numInterfaces; j++) {
        const Interface *superInterface = interfaces[j];
        addInterfaceToVTable(superInterface, vtable, numEntries);
    }
    
    /* Check for interface table overflow */
    if (!build(interfaceDispatchTable))
        verifyError(VerifyError::resourceExhausted);
    
    /* Now put in the methods defined in this class */
    for (int i = 0; i < methodCount; i++) {
        Method *method = const_cast<Method*>(methods[i]);
        const char *name = method->getName();
        
        /* No Vtable entries for <init> and <clinit> */
        if (name == initp || name == clinitp)
            continue;
        
        /* Private methods are dispatched statically, as are static methods. */
        Uint16 accessFlags = method->getModifiers();
        if (accessFlags & (CR_METHOD_PRIVATE | CR_METHOD_STATIC))
            continue;
        
        /* See if we've already assigned a VIndex to this method */
        bool VIndexSet = method->getVIndex(vIndex);
        
        /* Final methods that are not defined in our superclasses
        can also be resolved statically */
        Uint16 classFlags = reader->getAccessFlags();
        if (VIndexSet && ((accessFlags & CR_METHOD_FINAL) || (classFlags & CR_ACC_FINAL)))
            continue;
        
        /* This method is never dispatched statically */
        method->setDynamicallyDispatched(true);
        
        /* Do nothing if this method already has been assigned a VTable offset,
           i.e. because it overrides a method in a superclass or because it is
           used in an interface. */
        if (VIndexSet)
            continue;
        
        vtable[numEntries].method = method;
        method->setVIndex(numEntries);
        addVIndexForMethod(method, numEntries);
        
        numEntries++;
    }
    
    if (!(getAccessFlags() & CR_ACC_INTERFACE)) {
        /* We need an additional entry for a self-referential pointer to the class */
        vtable[numEntries].summary = this;
        numEntries++;
    }
    
    numVTableEntries = numEntries;
    
	UT_LOG(ClassFileSummary, PR_LOG_DEBUG, ("\t<%s> : %d vtable entries\n", 
        reader->getThisClass()->getUtf()->getUtfString(), 
        numVTableEntries));
    
#if 0
    dumpVTable();
#endif
}

#ifdef DEBUG_LOG
void ClassFileSummary::dumpVTable()
{
	for (Uint32 i=0; i < numVTableEntries; i++) {
		Method *method = vtable[i].method;
		if (method) {
			UT_LOG(ClassFileSummary, PR_LOG_ALWAYS, ("\t\t%s\n", method->toString()));
		} else {
			ClassFileSummary *summary = vtable[i].summary;
			assert(summary);
			UT_LOG(ClassFileSummary, PR_LOG_ALWAYS, ("\t\tType: %s\n", summary->getClassName()));
		}
	}
}
#endif   // DEBUG_LOG
				
void ClassFileSummary::addVIndexForMethod(Method *method, Uint32 vIndex) {
    Vector<Uint32> * vIndices = NULL;
    vIndicesUsed.get(method, &vIndices);
    if (!vIndices) {
        vIndices = new Vector<Uint32>(2);
        vIndicesUsed.add(method, vIndices);
    }
    vIndices->append(vIndex);
}
  


/* Return a pointer to the method that is used to implement an
   interface for a given class and interface method.  The implementation
   method is determined using the same tables and algorithm used by the
   compiled Java code.  If the method cannot be statically determined,
   i.e. because it must be looked up in the class' vtable, then no
   method is returned and vIndex is set to the vtable index of the
   implementation. */
const Method *
ClassFileSummary::lookupImplementationOfInterfaceMethod(const Method *interfaceMethod,
                                                        Uint32 &vIndex, 
                                                        addr &a)
{
    const Interface *iface = &asInterface(*interfaceMethod->getDeclaringClass());
    ClassFileSummary *interfaceSummary = iface->summary;

    /* Get the unique serial number of the interface */
    Uint32 interfaceNumber = iface->getInterfaceNumber();
    assert(interfaceNumber < InterfaceDispatchTable::maxInterfaces);
    
    /* Get offset, in bytes, from the start of this class' vtable to the 
    start of the given interface's sub-vtable */
    Type *t = static_cast<Type *>(getThisClass());
    VTableOffset interfaceVTableOffset = *(t->interfaceVIndexTable + interfaceNumber);
    
    /* Convert from vtable byte offset to vtable index */
    Uint32 interfaceVTableIndex = interfaceVTableOffset / sizeof(VTableOffset);
 
    /* Check that the given class actually implements the specified interface, using the
       same technique as the compiled Java code. The last entry of each sub-table
       should be a pointer to the given Interface object. */
    Uint32 interfacePointerVIndex = interfaceVTableIndex + interfaceSummary->numVTableEntries - 1;
    if ((interfacePointerVIndex >= numVTableEntries) ||
        (vtable[interfacePointerVIndex].summary != this)) {
        verifyError(VerifyError::noSuchMethod);
        return NULL; // NOTREACHED
    }

    /* Lookup the method within the interface's sub-vtable */
    Uint32 interfaceMethodVIndex;
    bool hasVIndex = interfaceMethod->getVIndex(interfaceMethodVIndex);
    assert(hasVIndex);
    const Method *method;

    method = vtable[interfaceVTableIndex + interfaceMethodVIndex].method;
    a = const_cast<Method *>(method)->getCode();
    if (method->getDynamicallyDispatched()) {
        method->getVIndex(vIndex);
        return NULL;
    }
    return method;
}
  
/* Returns true if the current class is a subclass of the class
 * represented by summary
 */
bool ClassFileSummary::isSubClassOf(ClassFileSummary *summary)
{
  /* XXX What about array classes? */
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  for (ClassFileSummary *parent = parentInfo; parent; 
	   parent = parent->getParentInfo())
	if (parent == summary)
	  return true;

  return false;
}

Type *ClassFileSummary::getSuperClass() const {
  return const_cast<Type*>(getThisClass()->getSuperClass());
}

/* Returns the class object corresponding to the supertype
 * of this class.  The supertype, which is used for computing
 * assignability, provides a more detailed type taxonomy
 * than the superclass.
 *
 * For all objects, except arrays and interfaces, the supertype
 * is the same as the superclass.
 */
Type *ClassFileSummary::getSuperType() const
{
  return const_cast<Type*>(getThisClass()->getSuperClass());
}

/* Returns total space taken up by this object. For array classes,
 * returns total space taken up by the component class.
 */
Uint32 ClassFileSummary::getObjSize() const 
{
  /* XXX Is this right? */
  if (arrayType)
	return sizeof(JavaArray);

  return totalSize;
}

/* returns the constantpool of the class For array classes,
 * returns the constantPool of the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
const ConstantPool *ClassFileSummary::getConstantPool() const 
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return constantPool;
}

/* Returns number of items in the constant pool of this class 
 * For array classes, returns the constant pool count of the
 * component class. If this function is called on a primitive array, 
 * throws a VerifyError.
 */
Uint32 ClassFileSummary::getConstantPoolCount() const 
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return nConstants;
}
  
/* returns an array of Interfaces that this class directly implements.
 * For array classes, returns the interfaces of the component class.
 * If this is called  on a primitive array, throws a VerifyError.
 */
const Interface **ClassFileSummary::getInterfaces()
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  if (!interfaces.built()) buildInterfaces();
  
  return (const Interface **) interfaces.getEntries(); 
}

/* Returns number of interfaces that this class directly implements.
 * For array classes, returns the number of interfaces implemented by
 * the component class. If this function is called on a primitive array, 
 * throws a VerifyError.
 */
Uint16 ClassFileSummary::getInterfaceCount() const
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);
  
  return reader->getInterfaceCount();
}

/* Returns an array of Field classes, each of which contains information 
 * about a field in the class. For array classes, returns fields of
 * the component class. If this function is called on a primitive array, 
 * throws a VerifyError.
 */
const Field **ClassFileSummary::getFields()
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  if (!fields.built()) buildFields();
  return (const Field **) fields.getEntries(); 
}

/* Returns number of fields in the class. For array classes, returns
 * the field count of the component class. If this
 * function is called on a primitive array,  throws a VerifyError.
 */
Uint16 ClassFileSummary::getFieldCount() const
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return reader->getFieldCount();
}

/* Returns an array of method classes, each of which contains reflection
 * information about a method in the class. For array classes, returns
 * the methods of the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
const Method **ClassFileSummary::getMethods()
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  if (!methods.built()) buildMethods();
  return (const Method **) methods.getEntries();
}


/* Returns an array of MethodInfo structures that contain static 
 * (compile-time) information about methods. For array classes,
 * returns MethodInfo's for the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */ 
const MethodInfo **ClassFileSummary::getMethodInfo() const
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return reader->getMethodInfo();
}

/* Returns the number of methods in this class. For array classes,
 * returns the number of methods in the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
Uint16 ClassFileSummary::getMethodCount() const 
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return reader->getMethodCount(); 
}

/* Returns an array of public fields in the class. For array classes,
 * returns the public fields in the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
Field **ClassFileSummary::getPublicFields()
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  if (!publicFields.built())
	buildPublicFields();
  
  return publicFields.getEntries();
}

/* Returns the number of public fields in the class. For array classes,
 * returns the number of public fields in the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
Uint16 ClassFileSummary::getPublicFieldCount()
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return (Uint16) publicFields.getSize();
}

/* Returns an array of public methods in the class. For array classes,
 * returns the public methods in the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
Method **ClassFileSummary::getPublicMethods()
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);
  
  if (!publicMethods.built())
	buildPublicMethods();
  
  return publicMethods.getEntries();
}

/* Returns the number of public methods in the class. For array classes,
 * returns the number of public methods in the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
Uint16 ClassFileSummary::getPublicMethodCount()
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return (Uint16) publicMethods.getSize();
}


/* Returns an array of declared methods in the class.  */
const Method **ClassFileSummary::getDeclaredMethods()
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);
  
  if (!declaredMethods.built())
	buildDeclaredMethods();
  
  return (const Method **) declaredMethods.getEntries();
}

/* Returns the number of declared methods in the class. */
Uint16 ClassFileSummary::getDeclaredMethodCount() 
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return (Uint16) declaredMethods.getSize();
}


/* Returns an array of constructors of the class. For array classes,
 * returns the constructors in the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
const Constructor **ClassFileSummary::getConstructors()
{
  /* XXX Handle array types correctly: they have one constructor! */
  if (arrayType)
	verifyError(VerifyError::illegalAccess);
  
  if (!constructors.built())
	buildConstructors();
  
  return (const Constructor **) constructors.getEntries();

}

/* Returns the number of constructors in the class. For array classes,
 * returns the number of constructors in the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
Uint16 ClassFileSummary::getConstructorCount()
{
  /* XXX Handle array types correctly: they have one constructor! */
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return (Uint16) constructors.getSize();

}

/* Returns an array of public constructors of the class. For array classes,
 * returns the public constructors in the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
Constructor **ClassFileSummary::getPublicConstructors()
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);
  
  if (!publicConstructors.built())
	buildPublicConstructors();
  
  return publicConstructors.getEntries();
}

/* Returns the number of public constructors in the class. For array 
 * classes, returns the number of constructors in the component class. 
 * If this function is called on a primitive array, throws a VerifyError.
 */
Uint16 ClassFileSummary::getPublicConstructorCount()
{
  /* XXX Handle array types correctly: they have one constructor! */
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return (Uint16) publicConstructors.getSize();
}


/* Returns an array of (global) attributes for this class.
 * For array classes, returns the attributes of the component class.
 * If this function is called on a primitive array, throws a VerifyError.
 */
const AttributeInfoItem **ClassFileSummary::getAttributeInfo() const
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return reader->getAttributeInfo();
}

/* Returns number of (global) attributes in this class. For array classes,
 * returns the number of attributes in the component class. If this
 * function is called on a primitive array, throws a VerifyError.
 */
Uint16 ClassFileSummary::getAttributeCount() const 
{
  if (arrayType)
	verifyError(VerifyError::illegalAccess);

  return reader->getAttributeCount();
}
  

/* Sets vt to the vtable of this class. Normally, this method
 * should not be of interest to anyone other than other ClassFileSummary
 * instances; it is used by ClassFileSummary to get its parent's vtable.
 * If called from an array class, returns the vtable of the component.
 * If this function is called on a primitive array, throws a VerifyError.
 */
Uint32 ClassFileSummary::getVTable(VTableNode *&vt) const
{
  /* need to handle array types */
  assert(!arrayType);

  vt = vtable; 
  return numVTableEntries;
}


