#include "mozilla/dom/IPDL.h"

#include "mozilla/dom/IPDLBinding.h"

#include "mozilla/ipdl/IPDLProtocol.h"
#include "mozilla/ipdl/IPDLProtocolInstance.h"
#include "mozilla/ipdl/ipc/PContentChildIPCInterface.h"
#include "mozilla/ipdl/ipc/PContentParentIPCInterface.h"
#include "nsReadableUtils.h"
#include "nsIObserverService.h"
#include "nsJSUtils.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentChild.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(IPDL)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IPDL)
  tmp->mTopLevelClassConstructor = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParsedProtocolClassTable)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTopLevelParentInstances)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTopLevelChildInstance)
  mozilla::DropJSObjects(tmp);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IPDL)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParsedProtocolClassTable)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTopLevelParentInstances)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTopLevelChildInstance)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IPDL)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mTopLevelClassConstructor)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(IPDL)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IPDL)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IPDL)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

IPDL::IPDL(nsIGlobalObject* aParentObject)
  : mParentObject(aParentObject)
  , mChildInterface(nullptr)
{
  mozilla::HoldJSObjects(this);

  // Register to listen to new Content notifications.
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->AddObserver(this, "ipc:content-created", /* ownsWeak */ true);
  }
}

/* virtual */
IPDL::~IPDL() {}

void
IPDL::RegisterTopLevelClass(JSContext* aCx,
                            JS::Handle<JSObject*> aClassConstructor)
{
  mTopLevelClassConstructor = aClassConstructor;

  Sequence<JS::HandleValue> emptyArray;

  // When we register the top level class, we want to trigger a content creation
  // event for all existing content protocol instances.
  for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
    NewTopLevelProtocolParent(aCx, emptyArray, cp);
  }

  if (ContentChild::GetSingleton()) {
    NewTopLevelProtocolChild(aCx, emptyArray);
  }
}

void
IPDL::GetTopLevelInstance(JSContext* aCx,
                          const Optional<uint32_t>& aId,
                          JS::MutableHandle<JSObject*> aRetval)
{
  // If we got an ID, then we return a parent instance, otherwise we return the
  // child instance.
  if (aId.WasPassed() && !ContentChild::GetSingleton()) {
    if (auto instance = mTopLevelParentInstances.GetWeak(aId.Value())) {
      aRetval.set(instance->GetInstanceObject());
    } else {
      JS_ReportErrorUTF8(aCx, "Unknown parent instance ID");
      return;
    }
  } else {
    if (mTopLevelChildInstance) {
      aRetval.set(mTopLevelChildInstance->GetInstanceObject());
    } else {
      JS_ReportErrorUTF8(aCx, "No top level child instance");
      return;
    }
  }
}

void
IPDL::GetTopLevelInstances(JSContext* aCx, nsTArray<JSObject*>& aRetval)
{
  // Return all instances depending on the side we're on.
  if (mTopLevelChildInstance) {
    aRetval.AppendElement(mTopLevelChildInstance->GetInstanceObject());
  } else {
    for (auto i = mTopLevelParentInstances.Iter(); !i.Done(); i.Next()) {
      aRetval.AppendElement(i.Data()->GetInstanceObject());
    }
  }
}

void
IPDL::RegisterProtocol(JSContext* aCx,
                       const nsAString& aProtocolName,
                       const nsAString& aProtocolURI,
                       bool aEagerLoad)
{
  mProtocolURIs.Put(aProtocolName, nsString(aProtocolURI));
  if (aEagerLoad) {
    JS::RootedObject wrapper(aCx, GetWrapper());
    auto lookup = mParsedProtocolClassTable.LookupForAdd(aProtocolName);
    if (!lookup) {
      RefPtr<ipdl::IPDLProtocol> newIPDLProtocol = NewIPDLProtocol(aProtocolName, wrapper, aCx);

      if (!newIPDLProtocol) {
        lookup.OrRemove();
        return;
      }

      lookup.OrInsert([&newIPDLProtocol]() { return std::move(newIPDLProtocol); });
    }
  }
}

bool
IPDL::DoResolve(JSContext* aCx,
                JS::Handle<JSObject*> aObj,
                JS::Handle<jsid> aId,
                JS::MutableHandle<JS::PropertyDescriptor> aRv)
{
  // If it's not an ID, skip this.
  if (!JSID_IS_STRING(aId)) {
    return true;
  }

  // Get the property name.
  nsAutoJSString propertyName;
  if (!propertyName.init(aCx, JSID_TO_STRING(aId))) {
    return true;
  }

  // Check it's not a real existing property.
  if (IsRealProperty(propertyName)) {
    return true;
  }

  // Otherwise, resolve the protocol property.
  return ResolveProtocolProperty(propertyName, aObj, aCx, aRv);
}

void
IPDL::GetOwnPropertyNames(JSContext* aCx,
                          JS::AutoIdVector& aNames,
                          bool aEnumerableOnly,
                          mozilla::ErrorResult& aRv)
{
  if (aEnumerableOnly) {
    return;
  }

  // We want to return the real properties + the existing protocol names we
  // already parsed.
  if (!aNames.initCapacity(mParsedProtocolIDs.Capacity() +
                           sPropertyNamesLength)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  // We add the protocol names.
  JS::RootedId id(aCx);
  for (auto& name : mParsedProtocolIDs) {
    if (!JS_CharsToId(aCx, JS::TwoByteChars(name.get(), name.Length()), &id)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    if (!aNames.append(id)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }

  // We add the real properties.
  for (auto& propertyName : sPropertyNames) {
    if (!JS_CharsToId(
          aCx,
          JS::TwoByteChars(propertyName.get(), propertyName.Length()),
          &id)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    if (!aNames.append(id)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
  }
}

nsISupports*
IPDL::GetParentObject()
{
  return mParentObject;
}

/* static */ bool
IPDL::MayResolve(jsid aId)
{
  return true;
}

/* virtual */ JSObject*
IPDL::WrapObject(JSContext* aCx, JS::HandleObject aGivenProto)
{
  return IPDL_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */ already_AddRefed<IPDL>
IPDL::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<IPDL> ipdl = new IPDL(global);
  return ipdl.forget();
}

bool
IPDL::IsRealProperty(const nsAString& aPropertyName)
{
  // Simple loop through the real property names.
  for (auto& propertyName : sPropertyNames) {
    if (aPropertyName.Equals(propertyName)) {
      return true;
    }
  }

  return false;
}

bool
IPDL::ResolveProtocolProperty(const nsString& aProtocolPropertyName,
                              JS::Handle<JSObject*> aObj,
                              JSContext* aCx,
                              JS::MutableHandle<JS::PropertyDescriptor> aRv)
{
  // If we haven't parsed this protocol yet, do it and create a new IPDLProtocol
  // object.
  auto ipdlProtocol = mParsedProtocolClassTable.LookupForAdd(aProtocolPropertyName);
  if (!ipdlProtocol) {
    RefPtr<ipdl::IPDLProtocol> newIPDLProtocol = NewIPDLProtocol(aProtocolPropertyName, aObj, aCx);

    if (!newIPDLProtocol) {
      ipdlProtocol.OrRemove();
      return false;
    }

    ipdlProtocol.OrInsert([&newIPDLProtocol]() { return std::move(newIPDLProtocol); });
  }

  aRv.object().set(aObj);
  aRv.value().setObject(*ipdlProtocol.Data()->GetProtocolClassConstructor());
  aRv.setAttributes(0);
  aRv.setGetter(nullptr);
  aRv.setSetter(nullptr);

  return true;
}

Maybe<ipdl::IPDLSide>
IPDL::GetProtocolSide(const nsAString& aProtocolName)
{
  NS_NAMED_LITERAL_STRING(parentLiteral, "Parent");
  NS_NAMED_LITERAL_STRING(childLiteral, "Child");

  // Find the parent suffix.
  if (StringEndsWith(aProtocolName, parentLiteral)) {
    return Some(ipdl::IPDLSide::Parent);
  }

  // Find the child suffix.
  if (StringEndsWith(aProtocolName, childLiteral)) {
    return Some(ipdl::IPDLSide::Child);
  }

  return Nothing();
}

void
IPDL::StripParentSuffix(const nsAString& aProtocolName, nsAString& aRetVal)
{
  NS_NAMED_LITERAL_STRING(parentLiteral, "Parent");
  aRetVal = Substring(
    aProtocolName, 0, aProtocolName.Length() - parentLiteral.Length());
}

void
IPDL::StripChildSuffix(const nsAString& aProtocolName, nsAString& aRetVal)
{
  NS_NAMED_LITERAL_STRING(childLiteral, "Child");
  aRetVal =
    Substring(aProtocolName, 0, aProtocolName.Length() - childLiteral.Length());
}

ipdl::ipc::PContentChildIPCInterface*
IPDL::GetOrInitChildInterface()
{
  if (!mChildInterface && ContentChild::GetSingleton()) {
    mChildInterface.reset(new ipdl::ipc::PContentChildIPCInterface(
      this, ContentChild::GetSingleton()));
  }

  return mChildInterface.get();
}

already_AddRefed<ipdl::IPDLProtocol>
IPDL::NewIPDLProtocol(const nsAString& aProtocolName,
                      JS::Handle<JSObject*> aObj,
                      JSContext* aCx)
{
  auto ipdlSide = GetProtocolSide(aProtocolName);

  nsAutoString unsidedProtocolName;
  // Assign a name depending on the protocol side.
  if (!ipdlSide) {
    return nullptr;
  }

  switch (*ipdlSide) {
    case ipdl::IPDLSide::Parent:
      StripParentSuffix(aProtocolName, unsidedProtocolName);
      break;

    case ipdl::IPDLSide::Child:
      GetOrInitChildInterface();
      StripChildSuffix(aProtocolName, unsidedProtocolName);
      break;

    default:
      return nullptr;
  }

  if (!mParsedProtocolIDs.AppendElement(aProtocolName)) {
    return nullptr;
  }

  if (!mProtocolURIs.Contains(unsidedProtocolName)) {
    return nullptr;
  }

  // Create our new IPDLProtocol object and return it.
  auto newIPDLProtocol = MakeRefPtr<ipdl::IPDLProtocol>(
    this,
    *ipdlSide,
    NS_ConvertUTF16toUTF8(mProtocolURIs.Get(unsidedProtocolName)),
    xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx)),
    aObj,
    aCx);

  // Some error during parsing or construction.
  if (!newIPDLProtocol->GetProtocolClassConstructor()) {
    return nullptr;
  }

  return newIPDLProtocol.forget();
}

JSObject*
IPDL::NewTopLevelProtocolParent(JSContext* aCx,
                                const Sequence<JS::Handle<JS::Value>>& aArgs,
                                ContentParent* aCp)
{
  // If we don't already have an IPCInterface for this content parent, create
  // it.
  auto parentInterface = mParentInterfaces.LookupForAdd(aCp->ChildID()).OrInsert([this, aCp]() {
    return new ipdl::ipc::PContentParentIPCInterface(this, aCp);
  });

  JS::RootedObject constructor(aCx, mTopLevelClassConstructor.get());

  // Prepare the arguments into a vector.
  JS::AutoValueVector argVector(aCx);
  if (!argVector.initCapacity(aArgs.Length())) {
    JS_ReportErrorUTF8(aCx, "Couldn't initialize argument vector");
    return nullptr;
  }

  for (auto& arg : aArgs) {
    if (!argVector.append(arg.get())) {
      JS_ReportErrorUTF8(aCx, "Couldn't add argument to arg vector");
      return nullptr;
    }
  }

  // Create the protocol instance, add the IPCInterface to the
  // IPDLProtocolInstance...
  JS::RootedObject topLevelProtocol(aCx, JS_New(aCx, constructor, argVector));
  auto* instance =
    static_cast<ipdl::IPDLProtocolInstance*>(JS_GetPrivate(topLevelProtocol.get()));

  if (!instance) {
    JS_ReportErrorUTF8(aCx, "Couldn't get protocol instance object from private date field");
    return nullptr;
  }

  instance->SetIPCInterface(parentInterface);

  mTopLevelParentInstances.Put(aCp->ChildID(), instance);

  return topLevelProtocol;
}

JSObject*
IPDL::NewTopLevelProtocolChild(JSContext* aCx,
                               const Sequence<JS::Handle<JS::Value>>& aArgs)
{
  JS::RootedObject constructor(aCx, mTopLevelClassConstructor.get());

  // Prepare the arguments into a vector.
  JS::AutoValueVector argVector(aCx);
  if (!argVector.initCapacity(aArgs.Length())) {
    JS_ReportErrorUTF8(aCx, "Couldn't initialize argument vector");
    return nullptr;
  }

  for (auto& arg : aArgs) {
    if (!argVector.append(arg.get())) {
      JS_ReportErrorUTF8(aCx, "Couldn't add argument to arg vector");
      return nullptr;
    }
  }

  // Create the protocol instance, add the IPCInterface to the
  // IPDLProtocolInstance...
  JS::RootedObject topLevelProtocol(aCx, JS_New(aCx, constructor, argVector));
  auto* instance =
    static_cast<ipdl::IPDLProtocolInstance*>(JS_GetPrivate(topLevelProtocol.get()));

  if (!instance) {
    JS_ReportErrorUTF8(aCx, "Couldn't get protocol instance object from private date field");
    return nullptr;
  }

  instance->SetIPCInterface(GetOrInitChildInterface());

  mTopLevelChildInstance = instance;

  return topLevelProtocol;
}

NS_IMETHODIMP
IPDL::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  nsDependentCString topic(aTopic);
  if (topic.EqualsLiteral("ipc:content-created")) {
    nsCOMPtr<nsIContentParent> cp = do_QueryInterface(aSubject);
    AutoEntryScript aes(mParentObject, "New Content Parent");
    NewTopLevelProtocolParent(
      aes.cx(), Sequence<JS::HandleValue>(), cp->AsContentParent());
  }

  return NS_OK;
}

constexpr nsLiteralString IPDL::sPropertyNames[];
constexpr size_t IPDL::sPropertyNamesLength;

} // namespace dom
} // namespace mozilla
