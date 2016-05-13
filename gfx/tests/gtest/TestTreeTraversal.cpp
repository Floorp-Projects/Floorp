#include <vector>
#include "mozilla/RefPtr.h"
#include "gtest/gtest.h"
#include "TreeTraversal.h"

using namespace mozilla::layers;
using namespace mozilla;

enum class SearchNodeType {Needle, Hay};
enum class ForEachNodeType {Continue, Skip};

template <class T>
class TestNodeBase {
  public:
    explicit TestNodeBase(T aType, int aExpectedTraversalRank = -1);
    void SetActualTraversalRank(int aRank);
    int GetExpectedTraversalRank();
    int GetActualTraversalRank();
    T GetType();
  private:
    int mExpectedTraversalRank;
    int mActualTraversalRank;
    T mType;
  protected:
    ~TestNodeBase<T>() {};
};

template <class T>
class TestNodeReverse : public TestNodeBase<T> {
  public:
    NS_INLINE_DECL_REFCOUNTING(TestNodeReverse<T>);
    explicit TestNodeReverse(T aType, int aExpectedTraversalRank = -1);
    void AddChild(RefPtr<TestNodeReverse<T>> aNode);
    TestNodeReverse<T>* GetLastChild();
    TestNodeReverse<T>* GetPrevSibling();
  private:
    void SetPrevSibling(RefPtr<TestNodeReverse<T>> aNode);
    void SetLastChild(RefPtr<TestNodeReverse<T>> aNode);
    RefPtr<TestNodeReverse<T>> mSiblingNode;
    RefPtr<TestNodeReverse<T>> mLastChildNode;
    ~TestNodeReverse<T>() {};
};

template <class T>
class TestNodeForward : public TestNodeBase<T> {
  public:
    NS_INLINE_DECL_REFCOUNTING(TestNodeForward<T>);
    explicit TestNodeForward(T aType, int aExpectedTraversalRank = -1);
    void AddChild(RefPtr<TestNodeForward<T>> aNode);
    TestNodeForward<T>* GetFirstChild();
    TestNodeForward<T>* GetNextSibling();
  private:
    void SetNextSibling(RefPtr<TestNodeForward<T>> aNode);
    void SetLastChild(RefPtr<TestNodeForward<T>> aNode);
    void SetFirstChild(RefPtr<TestNodeForward<T>> aNode);
    RefPtr<TestNodeForward<T>> mSiblingNode = nullptr;
    RefPtr<TestNodeForward<T>> mFirstChildNode = nullptr;
    // Track last child to facilitate appending children
    RefPtr<TestNodeForward<T>> mLastChildNode = nullptr;
    ~TestNodeForward<T>() {};
};

template <class T>
TestNodeReverse<T>::TestNodeReverse(T aType, int aExpectedTraversalRank) :
  TestNodeBase<T>(aType, aExpectedTraversalRank)
{

}

template <class T>
void TestNodeReverse<T>::SetLastChild(RefPtr<TestNodeReverse<T>> aNode)
{
  mLastChildNode = aNode;
}

template <class T>
void TestNodeReverse<T>::AddChild(RefPtr<TestNodeReverse<T>> aNode)
{
  aNode->SetPrevSibling(mLastChildNode);
  SetLastChild(aNode);
}

template <class T>
void TestNodeReverse<T>::SetPrevSibling(RefPtr<TestNodeReverse<T>> aNode)
{
  mSiblingNode = aNode;
}

template <class T>
TestNodeReverse<T>* TestNodeReverse<T>::GetLastChild()
{
  return mLastChildNode;
}

template <class T>
TestNodeReverse<T>* TestNodeReverse<T>::GetPrevSibling()
{
  return mSiblingNode;
}

template <class T>
TestNodeForward<T>::TestNodeForward(T aType, int aExpectedTraversalRank) :
  TestNodeBase<T>(aType, aExpectedTraversalRank)
{

}

template <class T>
void TestNodeForward<T>::AddChild(RefPtr<TestNodeForward<T>> aNode)
{
  if (mFirstChildNode == nullptr) {
    SetFirstChild(aNode);
    SetLastChild(aNode);
  }
  else {
    mLastChildNode->SetNextSibling(aNode);
    SetLastChild(aNode);
  }
}

template <class T>
void TestNodeForward<T>::SetLastChild(RefPtr<TestNodeForward<T>> aNode)
{
  mLastChildNode = aNode;
}

template <class T>
void TestNodeForward<T>::SetFirstChild(RefPtr<TestNodeForward<T>> aNode)
{
  mFirstChildNode = aNode;
}

template <class T>
void TestNodeForward<T>::SetNextSibling(RefPtr<TestNodeForward<T>> aNode)
{
  mSiblingNode = aNode;
}

template <class T>
TestNodeForward<T>* TestNodeForward<T>::GetFirstChild()
{
  return mFirstChildNode;
}

template <class T>
TestNodeForward<T>* TestNodeForward<T>::GetNextSibling()
{
  return mSiblingNode;
}

template <class T>
TestNodeBase<T>::TestNodeBase(T aType, int aExpectedTraversalRank):
    mExpectedTraversalRank(aExpectedTraversalRank),
    mActualTraversalRank(-1),
    mType(aType)
{
}

template <class T>
int TestNodeBase<T>::GetActualTraversalRank()
{
  return mActualTraversalRank;
}

template <class T>
void TestNodeBase<T>::SetActualTraversalRank(int aRank)
{
  mActualTraversalRank = aRank;
}

template <class T>
int TestNodeBase<T>::GetExpectedTraversalRank()
{
  return mExpectedTraversalRank;
}

template <class T>
T TestNodeBase<T>::GetType()
{
  return mType;
}

typedef TestNodeReverse<SearchNodeType> SearchTestNodeReverse;
typedef TestNodeReverse<ForEachNodeType> ForEachTestNodeReverse;
typedef TestNodeForward<SearchNodeType> SearchTestNodeForward;
typedef TestNodeForward<ForEachNodeType> ForEachTestNodeForward;

TEST(TreeTraversal, DepthFirstSearchNull)
{
  RefPtr<SearchTestNodeReverse> nullNode;
  RefPtr<SearchTestNodeReverse> result = DepthFirstSearch<layers::ReverseIterator>(nullNode.get(),
      [](SearchTestNodeReverse* aNode)
      {
        return aNode->GetType() == SearchNodeType::Needle;
      });
  ASSERT_EQ(result.get(), nullptr) << "Null root did not return null search result.";
}

TEST(TreeTraversal, DepthFirstSearchValueExists)
{
  int visitCount = 0;
  size_t expectedNeedleTraversalRank = 7;
  RefPtr<SearchTestNodeForward> needleNode;
  std::vector<RefPtr<SearchTestNodeForward>> nodeList;
  for (size_t i = 0; i < 10; i++)
  {
    if (i == expectedNeedleTraversalRank) {
      needleNode = new SearchTestNodeForward(SearchNodeType::Needle, i);
      nodeList.push_back(needleNode);
    } else if (i < expectedNeedleTraversalRank) {
      nodeList.push_back(new SearchTestNodeForward(SearchNodeType::Hay, i));
    } else {
      nodeList.push_back(new SearchTestNodeForward(SearchNodeType::Hay));
    }
  }

  RefPtr<SearchTestNodeForward> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);

  RefPtr<SearchTestNodeForward> foundNode = DepthFirstSearch<layers::ForwardIterator>(root.get(),
      [&visitCount](SearchTestNodeForward* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode, needleNode) << "Search did not return expected node.";
  ASSERT_EQ(foundNode->GetType(), SearchNodeType::Needle)
      << "Returned node does not match expected value (something odd happened).";
}

TEST(TreeTraversal, DepthFirstSearchValueExistsReverse)
{
  int visitCount = 0;
  size_t expectedNeedleTraversalRank = 7;
  RefPtr<SearchTestNodeReverse> needleNode;
  std::vector<RefPtr<SearchTestNodeReverse>> nodeList;
  for (size_t i = 0; i < 10; i++)
  {
    if (i == expectedNeedleTraversalRank) {
      needleNode = new SearchTestNodeReverse(SearchNodeType::Needle, i);
      nodeList.push_back(needleNode);
    } else if (i < expectedNeedleTraversalRank) {
      nodeList.push_back(new SearchTestNodeReverse(SearchNodeType::Hay, i));
    } else {
      nodeList.push_back(new SearchTestNodeReverse(SearchNodeType::Hay));
    }
  }

  RefPtr<SearchTestNodeReverse> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[9]);
  nodeList[7]->AddChild(nodeList[8]);

  RefPtr<SearchTestNodeReverse> foundNode = DepthFirstSearch<layers::ReverseIterator>(root.get(),
      [&visitCount](SearchTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode, needleNode) << "Search did not return expected node.";
  ASSERT_EQ(foundNode->GetType(), SearchNodeType::Needle)
      << "Returned node does not match expected value (something odd happened).";
}

TEST(TreeTraversal, DepthFirstSearchRootIsNeedle)
{
  RefPtr<SearchTestNodeReverse> root = new SearchTestNodeReverse(SearchNodeType::Needle, 0);
  RefPtr<SearchTestNodeReverse> childNode1= new SearchTestNodeReverse(SearchNodeType::Hay);
  RefPtr<SearchTestNodeReverse> childNode2 = new SearchTestNodeReverse(SearchNodeType::Hay);
  int visitCount = 0;
  RefPtr<SearchTestNodeReverse> result = DepthFirstSearch<layers::ReverseIterator>(root.get(),
      [&visitCount](SearchTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });
  ASSERT_EQ(result, root) << "Search starting at needle did not return needle.";
  ASSERT_EQ(root->GetExpectedTraversalRank(), root->GetActualTraversalRank())
      << "Search starting at needle did not return needle.";
  ASSERT_EQ(childNode1->GetExpectedTraversalRank(),
      childNode1->GetActualTraversalRank())
      << "Search starting at needle continued past needle.";
  ASSERT_EQ(childNode2->GetExpectedTraversalRank(),
      childNode2->GetActualTraversalRank())
      << "Search starting at needle continued past needle.";
}

TEST(TreeTraversal, DepthFirstSearchValueDoesNotExist)
{
  int visitCount = 0;
  std::vector<RefPtr<SearchTestNodeForward>> nodeList;
  for (int i = 0; i < 10; i++)
  {
      nodeList.push_back(new SearchTestNodeForward(SearchNodeType::Hay, i));
  }

  RefPtr<SearchTestNodeForward> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);


  RefPtr<SearchTestNodeForward> foundNode = DepthFirstSearch<layers::ForwardIterator>(root.get(),
      [&visitCount](SearchTestNodeForward* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (int i = 0; i < 10; i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode.get(), nullptr)
      << "Search found something that should not exist.";
}

TEST(TreeTraversal, DepthFirstSearchValueDoesNotExistReverse)
{
  int visitCount = 0;
  std::vector<RefPtr<SearchTestNodeReverse>> nodeList;
  for (int i = 0; i < 10; i++)
  {
      nodeList.push_back(new SearchTestNodeReverse(SearchNodeType::Hay, i));
  }

  RefPtr<SearchTestNodeReverse> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[9]);
  nodeList[7]->AddChild(nodeList[8]);


  RefPtr<SearchTestNodeReverse> foundNode = DepthFirstSearch<layers::ReverseIterator>(root.get(),
      [&visitCount](SearchTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (int i = 0; i < 10; i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode.get(), nullptr)
      << "Search found something that should not exist.";
}

TEST(TreeTraversal, DepthFirstSearchPostOrderNull)
{
  RefPtr<SearchTestNodeReverse> nullNode;
  RefPtr<SearchTestNodeReverse> result = DepthFirstSearchPostOrder<layers::ReverseIterator>(nullNode.get(),
      [](SearchTestNodeReverse* aNode)
      {
        return aNode->GetType() == SearchNodeType::Needle;
      });
  ASSERT_EQ(result.get(), nullptr) << "Null root did not return null search result.";
}

TEST(TreeTraversal, DepthFirstSearchPostOrderValueExists)
{
  int visitCount = 0;
  size_t expectedNeedleTraversalRank = 7;
  RefPtr<SearchTestNodeForward> needleNode;
  std::vector<RefPtr<SearchTestNodeForward>> nodeList;
  for (size_t i = 0; i < 10; i++)
  {
    if (i == expectedNeedleTraversalRank) {
      needleNode = new SearchTestNodeForward(SearchNodeType::Needle, i);
      nodeList.push_back(needleNode);
    } else if (i < expectedNeedleTraversalRank) {
      nodeList.push_back(new SearchTestNodeForward(SearchNodeType::Hay, i));
    } else {
      nodeList.push_back(new SearchTestNodeForward(SearchNodeType::Hay));
    }
  }

  RefPtr<SearchTestNodeForward> root = nodeList[9];
  nodeList[9]->AddChild(nodeList[2]);
  nodeList[9]->AddChild(nodeList[8]);
  nodeList[2]->AddChild(nodeList[0]);
  nodeList[2]->AddChild(nodeList[1]);
  nodeList[8]->AddChild(nodeList[6]);
  nodeList[8]->AddChild(nodeList[7]);
  nodeList[6]->AddChild(nodeList[5]);
  nodeList[5]->AddChild(nodeList[3]);
  nodeList[5]->AddChild(nodeList[4]);

  RefPtr<SearchTestNodeForward> foundNode = DepthFirstSearchPostOrder<layers::ForwardIterator>(root.get(),
      [&visitCount](SearchTestNodeForward* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode, needleNode) << "Search did not return expected node.";
  ASSERT_EQ(foundNode->GetType(), SearchNodeType::Needle)
      << "Returned node does not match expected value (something odd happened).";
}

TEST(TreeTraversal, DepthFirstSearchPostOrderValueExistsReverse)
{
  int visitCount = 0;
  size_t expectedNeedleTraversalRank = 7;
  RefPtr<SearchTestNodeReverse> needleNode;
  std::vector<RefPtr<SearchTestNodeReverse>> nodeList;
  for (size_t i = 0; i < 10; i++)
  {
    if (i == expectedNeedleTraversalRank) {
      needleNode = new SearchTestNodeReverse(SearchNodeType::Needle, i);
      nodeList.push_back(needleNode);
    } else if (i < expectedNeedleTraversalRank) {
      nodeList.push_back(new SearchTestNodeReverse(SearchNodeType::Hay, i));
    } else {
      nodeList.push_back(new SearchTestNodeReverse(SearchNodeType::Hay));
    }
  }

  RefPtr<SearchTestNodeReverse> root = nodeList[9];
  nodeList[9]->AddChild(nodeList[8]);
  nodeList[9]->AddChild(nodeList[2]);
  nodeList[2]->AddChild(nodeList[1]);
  nodeList[2]->AddChild(nodeList[0]);
  nodeList[8]->AddChild(nodeList[7]);
  nodeList[8]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[5]);
  nodeList[5]->AddChild(nodeList[4]);
  nodeList[5]->AddChild(nodeList[3]);

  RefPtr<SearchTestNodeReverse> foundNode = DepthFirstSearchPostOrder<layers::ReverseIterator>(root.get(),
      [&visitCount](SearchTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode, needleNode) << "Search did not return expected node.";
  ASSERT_EQ(foundNode->GetType(), SearchNodeType::Needle)
      << "Returned node does not match expected value (something odd happened).";
}

TEST(TreeTraversal, DepthFirstSearchPostOrderRootIsNeedle)
{
  RefPtr<SearchTestNodeReverse> root = new SearchTestNodeReverse(SearchNodeType::Needle, 0);
  RefPtr<SearchTestNodeReverse> childNode1= new SearchTestNodeReverse(SearchNodeType::Hay);
  RefPtr<SearchTestNodeReverse> childNode2 = new SearchTestNodeReverse(SearchNodeType::Hay);
  int visitCount = 0;
  RefPtr<SearchTestNodeReverse> result = DepthFirstSearchPostOrder<layers::ReverseIterator>(root.get(),
      [&visitCount](SearchTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });
  ASSERT_EQ(result, root) << "Search starting at needle did not return needle.";
  ASSERT_EQ(root->GetExpectedTraversalRank(), root->GetActualTraversalRank())
      << "Search starting at needle did not return needle.";
  ASSERT_EQ(childNode1->GetExpectedTraversalRank(),
      childNode1->GetActualTraversalRank())
      << "Search starting at needle continued past needle.";
  ASSERT_EQ(childNode2->GetExpectedTraversalRank(),
      childNode2->GetActualTraversalRank())
      << "Search starting at needle continued past needle.";
}

TEST(TreeTraversal, DepthFirstSearchPostOrderValueDoesNotExist)
{
  int visitCount = 0;
  std::vector<RefPtr<SearchTestNodeForward>> nodeList;
  for (int i = 0; i < 10; i++)
  {
      nodeList.push_back(new SearchTestNodeForward(SearchNodeType::Hay, i));
  }

  RefPtr<SearchTestNodeForward> root = nodeList[9];
  nodeList[9]->AddChild(nodeList[2]);
  nodeList[9]->AddChild(nodeList[8]);
  nodeList[2]->AddChild(nodeList[0]);
  nodeList[2]->AddChild(nodeList[1]);
  nodeList[8]->AddChild(nodeList[6]);
  nodeList[8]->AddChild(nodeList[7]);
  nodeList[6]->AddChild(nodeList[5]);
  nodeList[5]->AddChild(nodeList[3]);
  nodeList[5]->AddChild(nodeList[4]);

  RefPtr<SearchTestNodeForward> foundNode = DepthFirstSearchPostOrder<layers::ForwardIterator>(root.get(),
      [&visitCount](SearchTestNodeForward* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (int i = 0; i < 10; i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode.get(), nullptr)
      << "Search found something that should not exist.";
}

TEST(TreeTraversal, DepthFirstSearchPostOrderValueDoesNotExistReverse)
{
  int visitCount = 0;
  std::vector<RefPtr<SearchTestNodeReverse>> nodeList;
  for (int i = 0; i < 10; i++)
  {
      nodeList.push_back(new SearchTestNodeReverse(SearchNodeType::Hay, i));
  }

  RefPtr<SearchTestNodeReverse> root = nodeList[9];
  nodeList[9]->AddChild(nodeList[8]);
  nodeList[9]->AddChild(nodeList[2]);
  nodeList[2]->AddChild(nodeList[1]);
  nodeList[2]->AddChild(nodeList[0]);
  nodeList[8]->AddChild(nodeList[7]);
  nodeList[8]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[5]);
  nodeList[5]->AddChild(nodeList[4]);
  nodeList[5]->AddChild(nodeList[3]);

  RefPtr<SearchTestNodeReverse> foundNode = DepthFirstSearchPostOrder<layers::ReverseIterator>(root.get(),
      [&visitCount](SearchTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (int i = 0; i < 10; i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode.get(), nullptr)
      << "Search found something that should not exist.";
}

TEST(TreeTraversal, BreadthFirstSearchNull)
{
  RefPtr<SearchTestNodeReverse> nullNode;
  RefPtr<SearchTestNodeReverse> result = BreadthFirstSearch<layers::ReverseIterator>(nullNode.get(),
      [](SearchTestNodeReverse* aNode)
      {
        return aNode->GetType() == SearchNodeType::Needle;
      });
  ASSERT_EQ(result.get(), nullptr) << "Null root did not return null search result.";
}

TEST(TreeTraversal, BreadthFirstSearchRootIsNeedle)
{
  RefPtr<SearchTestNodeReverse> root = new SearchTestNodeReverse(SearchNodeType::Needle, 0);
  RefPtr<SearchTestNodeReverse> childNode1= new SearchTestNodeReverse(SearchNodeType::Hay);
  RefPtr<SearchTestNodeReverse> childNode2 = new SearchTestNodeReverse(SearchNodeType::Hay);
  int visitCount = 0;
  RefPtr<SearchTestNodeReverse> result = BreadthFirstSearch<layers::ReverseIterator>(root.get(),
      [&visitCount](SearchTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });
  ASSERT_EQ(result, root) << "Search starting at needle did not return needle.";
  ASSERT_EQ(root->GetExpectedTraversalRank(), root->GetActualTraversalRank())
      << "Search starting at needle did not return needle.";
  ASSERT_EQ(childNode1->GetExpectedTraversalRank(),
      childNode1->GetActualTraversalRank())
      << "Search starting at needle continued past needle.";
  ASSERT_EQ(childNode2->GetExpectedTraversalRank(),
      childNode2->GetActualTraversalRank())
      << "Search starting at needle continued past needle.";
}

TEST(TreeTraversal, BreadthFirstSearchValueExists)
{
  int visitCount = 0;
  size_t expectedNeedleTraversalRank = 7;
  RefPtr<SearchTestNodeForward> needleNode;
  std::vector<RefPtr<SearchTestNodeForward>> nodeList;
  for (size_t i = 0; i < 10; i++)
  {
    if (i == expectedNeedleTraversalRank) {
      needleNode = new SearchTestNodeForward(SearchNodeType::Needle, i);
      nodeList.push_back(needleNode);
    } else if (i < expectedNeedleTraversalRank) {
      nodeList.push_back(new SearchTestNodeForward(SearchNodeType::Hay, i));
    } else {
      nodeList.push_back(new SearchTestNodeForward(SearchNodeType::Hay));
    }
  }

  RefPtr<SearchTestNodeForward> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[2]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[1]->AddChild(nodeList[4]);
  nodeList[2]->AddChild(nodeList[5]);
  nodeList[2]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);

  RefPtr<SearchTestNodeForward> foundNode = BreadthFirstSearch<layers::ForwardIterator>(root.get(),
      [&visitCount](SearchTestNodeForward* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode, needleNode) << "Search did not return expected node.";
  ASSERT_EQ(foundNode->GetType(), SearchNodeType::Needle)
      << "Returned node does not match expected value (something odd happened).";
}

TEST(TreeTraversal, BreadthFirstSearchValueExistsReverse)
{
  int visitCount = 0;
  size_t expectedNeedleTraversalRank = 7;
  RefPtr<SearchTestNodeReverse> needleNode;
  std::vector<RefPtr<SearchTestNodeReverse>> nodeList;
  for (size_t i = 0; i < 10; i++)
  {
    if (i == expectedNeedleTraversalRank) {
      needleNode = new SearchTestNodeReverse(SearchNodeType::Needle, i);
      nodeList.push_back(needleNode);
    } else if (i < expectedNeedleTraversalRank) {
      nodeList.push_back(new SearchTestNodeReverse(SearchNodeType::Hay, i));
    } else {
      nodeList.push_back(new SearchTestNodeReverse(SearchNodeType::Hay));
    }
  }

  RefPtr<SearchTestNodeReverse> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[2]);
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[1]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[2]->AddChild(nodeList[6]);
  nodeList[2]->AddChild(nodeList[5]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[9]);
  nodeList[7]->AddChild(nodeList[8]);

  RefPtr<SearchTestNodeReverse> foundNode = BreadthFirstSearch<layers::ReverseIterator>(root.get(),
      [&visitCount](SearchTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode, needleNode) << "Search did not return expected node.";
  ASSERT_EQ(foundNode->GetType(), SearchNodeType::Needle)
      << "Returned node does not match expected value (something odd happened).";
}

TEST(TreeTraversal, BreadthFirstSearchValueDoesNotExist)
{
  int visitCount = 0;
  std::vector<RefPtr<SearchTestNodeForward>> nodeList;
  for (int i = 0; i < 10; i++)
  {
    nodeList.push_back(new SearchTestNodeForward(SearchNodeType::Hay, i));
  }

  RefPtr<SearchTestNodeForward> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[2]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[1]->AddChild(nodeList[4]);
  nodeList[2]->AddChild(nodeList[5]);
  nodeList[2]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);


  RefPtr<SearchTestNodeForward> foundNode = BreadthFirstSearch<layers::ForwardIterator>(root.get(),
      [&visitCount](SearchTestNodeForward* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
      ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
          nodeList[i]->GetActualTraversalRank())
          << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode.get(), nullptr)
      << "Search found something that should not exist.";
}

TEST(TreeTraversal, BreadthFirstSearchValueDoesNotExistReverse)
{
  int visitCount = 0;
  std::vector<RefPtr<SearchTestNodeReverse>> nodeList;
  for (int i = 0; i < 10; i++)
  {
    nodeList.push_back(new SearchTestNodeReverse(SearchNodeType::Hay, i));
  }

  RefPtr<SearchTestNodeReverse> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[2]);
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[1]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[2]->AddChild(nodeList[6]);
  nodeList[2]->AddChild(nodeList[5]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[9]);
  nodeList[7]->AddChild(nodeList[8]);


  RefPtr<SearchTestNodeReverse> foundNode = BreadthFirstSearch<layers::ReverseIterator>(root.get(),
      [&visitCount](SearchTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == SearchNodeType::Needle;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
      ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
          nodeList[i]->GetActualTraversalRank())
          << "Node at index " << i << " was hit out of order.";
  }

  ASSERT_EQ(foundNode.get(), nullptr)
      << "Search found something that should not exist.";
}

TEST(TreeTraversal, ForEachNodeNullStillRuns)
{
  RefPtr<ForEachTestNodeReverse> nullNode;
  ForEachNode<layers::ReverseIterator>(nullNode.get(),
    [](ForEachTestNodeReverse* aNode)
    {
      return TraversalFlag::Continue;
    });
}

TEST(TreeTraversal, ForEachNodeAllEligible)
{
  std::vector<RefPtr<ForEachTestNodeForward>> nodeList;
  int visitCount = 0;
  for (int i = 0; i < 10; i++)
  {
    nodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Continue,i));
  }

  RefPtr<ForEachTestNodeForward> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);


  ForEachNode<layers::ForwardIterator>(root.get(),
      [&visitCount](ForEachTestNodeForward* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
            ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }
}

TEST(TreeTraversal, ForEachNodeAllEligibleReverse)
{
  std::vector<RefPtr<ForEachTestNodeReverse>> nodeList;
  int visitCount = 0;
  for (int i = 0; i < 10; i++)
  {
    nodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Continue,i));
  }

  RefPtr<ForEachTestNodeReverse> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[9]);
  nodeList[7]->AddChild(nodeList[8]);


  ForEachNode<layers::ReverseIterator>(root.get(),
      [&visitCount](ForEachTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
            ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }
}

TEST(TreeTraversal, ForEachNodeSomeIneligibleNodes)
{
  std::vector<RefPtr<ForEachTestNodeForward>> expectedVisitedNodeList;
  std::vector<RefPtr<ForEachTestNodeForward>> expectedSkippedNodeList;
  int visitCount = 0;

  expectedVisitedNodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Continue, 0));
  expectedVisitedNodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Skip, 1));
  expectedVisitedNodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Continue, 2));
  expectedVisitedNodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Skip, 3));

  expectedSkippedNodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Continue));
  expectedSkippedNodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Continue));
  expectedSkippedNodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Skip));
  expectedSkippedNodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Skip));

  RefPtr<ForEachTestNodeForward> root = expectedVisitedNodeList[0];
  expectedVisitedNodeList[0]->AddChild(expectedVisitedNodeList[1]);
  expectedVisitedNodeList[0]->AddChild(expectedVisitedNodeList[2]);
  expectedVisitedNodeList[1]->AddChild(expectedSkippedNodeList[0]);
  expectedVisitedNodeList[1]->AddChild(expectedSkippedNodeList[1]);
  expectedVisitedNodeList[2]->AddChild(expectedVisitedNodeList[3]);
  expectedVisitedNodeList[3]->AddChild(expectedSkippedNodeList[2]);
  expectedVisitedNodeList[3]->AddChild(expectedSkippedNodeList[3]);

  ForEachNode<layers::ForwardIterator>(root.get(),
      [&visitCount](ForEachTestNodeForward* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
            ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  for (size_t i = 0; i < expectedVisitedNodeList.size(); i++)
  {
    ASSERT_EQ(expectedVisitedNodeList[i]->GetExpectedTraversalRank(),
        expectedVisitedNodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  for (size_t i = 0; i < expectedSkippedNodeList.size(); i++)
  {
    ASSERT_EQ(expectedSkippedNodeList[i]->GetExpectedTraversalRank(),
        expectedSkippedNodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << "was not expected to be hit.";
  }
}

TEST(TreeTraversal, ForEachNodeSomeIneligibleNodesReverse)
{
  std::vector<RefPtr<ForEachTestNodeReverse>> expectedVisitedNodeList;
  std::vector<RefPtr<ForEachTestNodeReverse>> expectedSkippedNodeList;
  int visitCount = 0;

  expectedVisitedNodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Continue, 0));
  expectedVisitedNodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Skip, 1));
  expectedVisitedNodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Continue, 2));
  expectedVisitedNodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Skip, 3));

  expectedSkippedNodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Continue));
  expectedSkippedNodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Continue));
  expectedSkippedNodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Skip));
  expectedSkippedNodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Skip));

  RefPtr<ForEachTestNodeReverse> root = expectedVisitedNodeList[0];
  expectedVisitedNodeList[0]->AddChild(expectedVisitedNodeList[2]);
  expectedVisitedNodeList[0]->AddChild(expectedVisitedNodeList[1]);
  expectedVisitedNodeList[1]->AddChild(expectedSkippedNodeList[1]);
  expectedVisitedNodeList[1]->AddChild(expectedSkippedNodeList[0]);
  expectedVisitedNodeList[2]->AddChild(expectedVisitedNodeList[3]);
  expectedVisitedNodeList[3]->AddChild(expectedSkippedNodeList[3]);
  expectedVisitedNodeList[3]->AddChild(expectedSkippedNodeList[2]);

  ForEachNode<layers::ReverseIterator>(root.get(),
      [&visitCount](ForEachTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
            ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  for (size_t i = 0; i < expectedVisitedNodeList.size(); i++)
  {
    ASSERT_EQ(expectedVisitedNodeList[i]->GetExpectedTraversalRank(),
        expectedVisitedNodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }

  for (size_t i = 0; i < expectedSkippedNodeList.size(); i++)
  {
    ASSERT_EQ(expectedSkippedNodeList[i]->GetExpectedTraversalRank(),
        expectedSkippedNodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << "was not expected to be hit.";
  }
}

TEST(TreeTraversal, ForEachNodeIneligibleRoot)
{
  int visitCount = 0;

  RefPtr<ForEachTestNodeReverse> root = new ForEachTestNodeReverse(ForEachNodeType::Skip, 0);
  RefPtr<ForEachTestNodeReverse> childNode1 = new ForEachTestNodeReverse(ForEachNodeType::Continue);
  RefPtr<ForEachTestNodeReverse> chlidNode2 = new ForEachTestNodeReverse(ForEachNodeType::Skip);

  ForEachNode<layers::ReverseIterator>(root.get(),
      [&visitCount](ForEachTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
            ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  ASSERT_EQ(root->GetExpectedTraversalRank(), root->GetActualTraversalRank())
      << "Root was hit out of order.";
  ASSERT_EQ(childNode1->GetExpectedTraversalRank(), childNode1->GetActualTraversalRank())
      << "Eligible child was still hit.";
  ASSERT_EQ(chlidNode2->GetExpectedTraversalRank(), chlidNode2->GetActualTraversalRank())
      << "Ineligible child was still hit.";
}

TEST(TreeTraversal, ForEachNodeLeavesIneligible)
{

  std::vector<RefPtr<ForEachTestNodeForward>> nodeList;
  int visitCount = 0;
  for (int i = 0; i < 10; i++)
  {
    if (i == 1 || i == 9) {
      nodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Skip, i));
    } else {
      nodeList.push_back(new ForEachTestNodeForward(ForEachNodeType::Continue, i));
    }
  }

  RefPtr<ForEachTestNodeForward> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[0]->AddChild(nodeList[2]);
  nodeList[2]->AddChild(nodeList[3]);
  nodeList[2]->AddChild(nodeList[4]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[8]);
  nodeList[7]->AddChild(nodeList[9]);

  ForEachNode<layers::ForwardIterator>(root.get(),
      [&visitCount](ForEachTestNodeForward* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
            ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }
}

TEST(TreeTraversal, ForEachNodeLeavesIneligibleReverse)
{

  std::vector<RefPtr<ForEachTestNodeReverse>> nodeList;
  int visitCount = 0;
  for (int i = 0; i < 10; i++)
  {
    if (i == 1 || i == 9) {
      nodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Skip, i));
    } else {
      nodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Continue, i));
    }
  }

  RefPtr<ForEachTestNodeReverse> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[2]);
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[2]->AddChild(nodeList[4]);
  nodeList[2]->AddChild(nodeList[3]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[9]);
  nodeList[7]->AddChild(nodeList[8]);

  ForEachNode<layers::ReverseIterator>(root.get(),
      [&visitCount](ForEachTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
        return aNode->GetType() == ForEachNodeType::Continue
            ? TraversalFlag::Continue : TraversalFlag::Skip;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }
}

TEST(TreeTraversal, ForEachNodeLambdaReturnsVoid)
{
  std::vector<RefPtr<ForEachTestNodeReverse>> nodeList;
  int visitCount = 0;
  for (int i = 0; i < 10; i++)
  {
    nodeList.push_back(new ForEachTestNodeReverse(ForEachNodeType::Continue,i));
  }

  RefPtr<ForEachTestNodeReverse> root = nodeList[0];
  nodeList[0]->AddChild(nodeList[4]);
  nodeList[0]->AddChild(nodeList[1]);
  nodeList[1]->AddChild(nodeList[3]);
  nodeList[1]->AddChild(nodeList[2]);
  nodeList[4]->AddChild(nodeList[6]);
  nodeList[4]->AddChild(nodeList[5]);
  nodeList[6]->AddChild(nodeList[7]);
  nodeList[7]->AddChild(nodeList[9]);
  nodeList[7]->AddChild(nodeList[8]);


  ForEachNode<layers::ReverseIterator>(root.get(),
      [&visitCount](ForEachTestNodeReverse* aNode)
      {
        aNode->SetActualTraversalRank(visitCount);
        visitCount++;
      });

  for (size_t i = 0; i < nodeList.size(); i++)
  {
    ASSERT_EQ(nodeList[i]->GetExpectedTraversalRank(),
        nodeList[i]->GetActualTraversalRank())
        << "Node at index " << i << " was hit out of order.";
  }
}
