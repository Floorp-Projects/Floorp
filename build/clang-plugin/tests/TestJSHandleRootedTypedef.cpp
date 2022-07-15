#include "js/TypeDecls.h"
#include "js/Value.h"
#include "js/GCVector.h"
#include "js/Id.h"

class Foo {
  void HandleFunction(JS::HandleFunction){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void HandleId(JS::HandleId){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void HandleObject(JS::HandleObject){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void HandleScript(JS::HandleScript){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void HandleString(JS::HandleString){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void HandleSymbol(JS::HandleSymbol){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void HandleBigInt(JS::HandleBigInt){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void HandleValue(JS::HandleValue){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void HandleValueVector(JS::HandleValueVector){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void HandleObjectVector(JS::HandleObjectVector){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void HandleIdVector(JS::HandleIdVector){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}

  void MutableHandleFunction(JS::MutableHandleFunction){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void MutableHandleId(JS::MutableHandleId){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void MutableHandleObject(JS::MutableHandleObject){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void MutableHandleScript(JS::MutableHandleScript){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void MutableHandleString(JS::MutableHandleString){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void MutableHandleSymbol(JS::MutableHandleSymbol){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void MutableHandleBigInt(JS::MutableHandleBigInt){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void MutableHandleValue(JS::MutableHandleValue){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void MutableHandleValueVector(JS::MutableHandleValueVector){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void MutableHandleObjectVector(JS::MutableHandleObjectVector){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  void MutableHandleIdVector(JS::MutableHandleIdVector){}; // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}

  // Examples of preferred forms
  void FullHandleFunction(JS::Handle<JSFunction*>){};
  void FullHandleId(JS::Handle<JS::PropertyKey>){};
  void FullHandleObject(JS::Handle<JSObject*>){};
  void FullHandleScript(JS::Handle<JSScript*>){};
  void FullHandleString(JS::Handle<JSString*>){};
  void FullHandleSymbol(JS::Handle<JS::Symbol*>){};
  void FullHandleBigInt(JS::Handle<JS::BigInt*>){};
  void FullHandleValue(JS::Handle<JS::Value>){};
  void FullHandleValueVector(JS::Handle<JS::StackGCVector<JS::Value>>){};
  void FullHandleObjectVector(JS::Handle<JS::StackGCVector<JSObject*>>){};
  void FullHandleIdVector(JS::HandleVector<JS::PropertyKey>){};
  void FullHandleValueVectorShorter(JS::HandleVector<JS::Value>){};
  void FullHandleObjectVectorShorter(JS::HandleVector<JSObject*>){};
  void FullHandleIdVectorShorter(JS::HandleVector<JS::PropertyKey>){};

  void FullMutableHandleFunction(JS::MutableHandle<JSFunction*>){};
  void FullMutableHandleId(JS::MutableHandle<JS::PropertyKey>){};
  void FullMutableHandleObject(JS::MutableHandle<JSObject*>){};
  void FullMutableHandleScript(JS::MutableHandle<JSScript*>){};
  void FullMutableHandleString(JS::MutableHandle<JSString*>){};
  void FullMutableHandleSymbol(JS::MutableHandle<JS::Symbol*>){};
  void FullMutableHandleBigInt(JS::MutableHandle<JS::BigInt*>){};
  void FullMutableHandleValue(JS::MutableHandle<JS::Value*>){};
  void FullMutableHandleValueVector(JS::MutableHandle<JS::StackGCVector<JS::Value>>){};
  void FullMutableHandleObjectVector(JS::MutableHandle<JS::StackGCVector<JSObject*>>){};
  void FullMutableHandleIdVector(JS::MutableHandle<JS::StackGCVector<JS::PropertyKey>>){};
  void FullMutableHandleValueVectorShorter(JS::MutableHandleVector<JS::Value>){};
  void FullMutableHandleObjectVectorShorter(JS::MutableHandleVector<JSObject*>){};
  void FullMutableHandleIdVectorShorter(JS::MutableHandleVector<JS::PropertyKey>){};
};

static void Bar(JSContext *aCx) {
  JS::RootedObject RootedObject(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::RootedFunction RootedFunction(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::RootedScript RootedScript(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::RootedString RootedString(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::RootedSymbol RootedSymbol(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::RootedBigInt RootedBigInt(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::RootedId RootedId(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::RootedValue RootedValue(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}

  JS::RootedValueVector RootedValueVector(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::RootedObjectVector RootedObjectVector(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::RootedIdVector RootedIdVector(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}

  JS::PersistentRootedFunction PersistentRootedFunction(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::PersistentRootedId PersistentRootedId(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::PersistentRootedObject PersistentRootedObject(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::PersistentRootedScript PersistentRootedScript(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::PersistentRootedString PersistentRootedString(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::PersistentRootedSymbol PersistentRootedSymbol(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::PersistentRootedBigInt PersistentRootedBigInt(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::PersistentRootedValue PersistentRootedValue(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}

  JS::PersistentRootedIdVector PersistentRootedIdVector(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::PersistentRootedObjectVector PersistentRootedObjectVector(aCx); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}

  JS::HandleFunction HandleFunction(RootedFunction); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::HandleId HandleId(RootedId); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::HandleObject HandleObject(RootedObject); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::HandleScript HandleScript(RootedScript); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::HandleString HandleString(RootedString); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::HandleSymbol HandleSymbol(RootedSymbol); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::HandleBigInt HandleBigInt(RootedBigInt); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::HandleValue HandleValue(RootedValue); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::HandleValueVector HandleValueVector(RootedValueVector); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::HandleObjectVector HandleObjectVector(RootedObjectVector); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::HandleIdVector HandleIdVector(RootedIdVector); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}

  JS::MutableHandleFunction MutableHandleFunction(&RootedFunction); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::MutableHandleId MutableHandleId(&RootedId); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::MutableHandleObject MutableHandleObject(&RootedObject); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::MutableHandleScript MutableHandleScript(&RootedScript); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::MutableHandleString MutableHandleString(&RootedString); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::MutableHandleSymbol MutableHandleSymbol(&RootedSymbol); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::MutableHandleBigInt MutableHandleBigInt(&RootedBigInt); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::MutableHandleValue MutableHandleValue(&RootedValue); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::MutableHandleValueVector MutableHandleValueVector(&RootedValueVector); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::MutableHandleObjectVector MutableHandleObjectVector(&RootedObjectVector); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}
  JS::MutableHandleIdVector MutableHandleIdVector(&RootedIdVector); // expected-error {{The fully qualified types are preferred over the shorthand typedefs for JS::Handle/JS::Rooted types outside SpiderMonkey.}}

  // Examples of preferred forms
  JS::Rooted<JSObject*> FullRootedObject(aCx);
  JS::Rooted<JSFunction*> FullRootedFunction(aCx);
  JS::Rooted<JSScript*> FullRootedScript(aCx);
  JS::Rooted<JSString*> FullRootedString(aCx);
  JS::Rooted<JS::Symbol*> FullRootedSymbol(aCx);
  JS::Rooted<JS::BigInt*> FullRootedBigInt(aCx);
  JS::Rooted<JS::PropertyKey> FullRootedId(aCx);
  JS::Rooted<JS::Value> FullRootedValue(aCx);

  JS::RootedVector<JS::Value> FullRootedValueVector(aCx);
  JS::RootedVector<JSObject*> FullRootedObjectVector(aCx);
  JS::RootedVector<JS::PropertyKey> FullRootedIdVector(aCx);

  JS::PersistentRooted<JSFunction*> FullPersistentRootedFunction(aCx);
  JS::PersistentRooted<JS::PropertyKey> FullPersistentRootedId(aCx);
  JS::PersistentRooted<JSObject*> FullPersistentRootedObject(aCx);
  JS::PersistentRooted<JSScript*> FullPersistentRootedScript(aCx);
  JS::PersistentRooted<JSString*> FullPersistentRootedString(aCx);
  JS::PersistentRooted<JS::Symbol*> FullPersistentRootedSymbol(aCx);
  JS::PersistentRooted<JS::BigInt*> FullPersistentRootedBigInt(aCx);
  JS::PersistentRooted<JS::Value> FullPersistentRootedValue(aCx);

  JS::PersistentRootedVector<JS::PropertyKey> FullPersistentRootedIdVector(aCx);
  JS::PersistentRootedVector<JSObject*> FullPersistentRootedObjectVector(aCx);

  JS::Handle<JSFunction*> FullHandleFunction(FullRootedFunction);
  JS::Handle<JS::PropertyKey> FullHandleId(FullRootedId);
  JS::Handle<JSObject*> FullHandleObject(FullRootedObject);
  JS::Handle<JSScript*> FullHandleScript(FullRootedScript);
  JS::Handle<JSString*> FullHandleString(FullRootedString);
  JS::Handle<JS::Symbol*> FullHandleSymbol(FullRootedSymbol);
  JS::Handle<JS::BigInt*> FullHandleBigInt(FullRootedBigInt);
  JS::Handle<JS::Value> FullHandleValue(FullRootedValue);
  JS::Handle<JS::StackGCVector<JS::Value>> FullHandleValueVector(FullRootedValueVector);
  JS::Handle<JS::StackGCVector<JSObject*>> FullHandleObjectVector(FullRootedObjectVector);
  JS::Handle<JS::StackGCVector<JS::PropertyKey>> FullHandleIdVector(FullRootedIdVector);
  JS::HandleVector<JS::Value> FullHandleValueVectorShorter(FullRootedValueVector);
  JS::HandleVector<JSObject*> FullHandleObjectVectorShorter(FullRootedObjectVector);
  JS::HandleVector<JS::PropertyKey> FullHandleIdVectorShorter(FullRootedIdVector);

  JS::MutableHandle<JSFunction*> FullMutableHandleFunction(&FullRootedFunction);
  JS::MutableHandle<JS::PropertyKey> FullMutableHandleId(&FullRootedId);
  JS::MutableHandle<JSObject*> FullMutableHandleObject(&FullRootedObject);
  JS::MutableHandle<JSScript*> FullMutableHandleScript(&FullRootedScript);
  JS::MutableHandle<JSString*> FullMutableHandleString(&FullRootedString);
  JS::MutableHandle<JS::Symbol*> FullMutableHandleSymbol(&FullRootedSymbol);
  JS::MutableHandle<JS::BigInt*> FullMutableHandleBigInt(&FullRootedBigInt);
  JS::MutableHandle<JS::Value> FullMutableHandleValue(&FullRootedValue);
  JS::MutableHandle<JS::StackGCVector<JS::Value>> FullMutableHandleValueVector(&FullRootedValueVector);
  JS::MutableHandle<JS::StackGCVector<JSObject*>> FullMutableHandleObjectVector(&FullRootedObjectVector);
  JS::MutableHandle<JS::StackGCVector<JS::PropertyKey>> FullMutableHandleIdVector(&FullRootedIdVector);
  JS::MutableHandleVector<JS::Value> FullMutableHandleValueVectorShorter(&FullRootedValueVector);
  JS::MutableHandleVector<JSObject*> FullMutableHandleObjectVectorShorter(&FullRootedObjectVector);
  JS::MutableHandleVector<JS::PropertyKey> FullMutableHandleIdVectorShorter(&FullRootedIdVector);
}
